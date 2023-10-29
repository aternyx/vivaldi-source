// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_proxying_webtransport.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/request_filter/filtered_request_info.h"
#include "content/public/browser/render_process_host.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace vivaldi {

namespace {

using network::mojom::WebTransportHandshakeClient;
using CreateCallback =
    content::ContentBrowserClient::WillCreateWebTransportCallback;

net::HttpRequestHeaders GetRequestHeaders() {
  // We don't attach certain headers:
  //  1. We cannot store pseudo-headers to `request_headers_` and they can be
  //     accessed via other ways, e.g., "url" for :scheme, :authority and
  //     :path.
  //  2. We don't attach the "origin" header, to be aligned with the usual
  //     loading case. Extension authors can use the "initiator" property to
  //     observe it.
  auto headers = net::HttpRequestHeaders();
  // TODO(1240935): Share the code with
  // DedicatedWebTransportHttp3Client::DoSendRequest.
  headers.SetHeader("sec-webtransport-http3-draft02", "1");
  return headers;
}

class WebTransportHandshakeProxy : public RequestFilterManager::Proxy,
                                   public WebTransportHandshakeClient {
 public:
  WebTransportHandshakeProxy(
      mojo::PendingRemote<WebTransportHandshakeClient> handshake_client,
      RequestFilterManager::RequestHandler* request_handler,
      RequestFilterManager::ProxySet& proxies,
      content::BrowserContext* browser_context,
      FilteredRequestInfo params,
      CreateCallback create_callback)
      : handshake_client_(std::move(handshake_client)),
        request_handler_(request_handler),
        proxies_(proxies),
        browser_context_(browser_context),
        info_(std::move(params)),
        create_callback_(std::move(create_callback)) {
    DCHECK(handshake_client_);
    DCHECK(create_callback_);
  }

  ~WebTransportHandshakeProxy() override {
    // This is important to ensure that no outstanding blocking requests
    // continue to reference state owned by this object.
    request_handler_->OnRequestWillBeDestroyed(browser_context_, &info_);
  }

  void Start() {
    bool should_collapse_initiator = false;
    // Since WebTransport doesn't support redirect, 'redirect_url' is ignored
    // even if extensions assigned it.
    const int result = request_handler_->OnBeforeRequest(
        browser_context_, &info_,
        base::BindOnce(&WebTransportHandshakeProxy::OnBeforeRequestCompleted,
                       base::Unretained(this)),
        &redirect_url_, &should_collapse_initiator);
    // It doesn't make sense to collapse WebTransport requests since they won't
    // be associated with a DOM element.
    DCHECK(!should_collapse_initiator);

    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    OnBeforeRequestCompleted(result);
  }

  void OnBeforeRequestCompleted(int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      // `this` is deleted.
      return;
    }

    request_headers_ = GetRequestHeaders();
    const int result = request_handler_->OnBeforeSendHeaders(
        browser_context_, &info_,
        base::BindOnce(
            &WebTransportHandshakeProxy::OnBeforeSendHeadersCompleted,
            base::Unretained(this)),
        &request_headers_, nullptr, nullptr);
    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    // See the comments in the OnBeforeSendHeadersCompleted to see why
    // we pass empty values.
    OnBeforeSendHeadersCompleted(result);
  }

  void OnBeforeSendHeadersCompleted(int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      // `this` is deleted.
      return;
    }

    // We don't allow extension authors to add/remove/change request headers,
    // as that may lead to a WebTransport over HTTP/3 protocol violation. We may
    // change this policy once https://github.com/w3c/webtransport/issues/263 is
    // resolved.
    request_handler_->OnSendHeaders(browser_context_, &info_,
                                    GetRequestHeaders());

    // Set up proxing.
    remote_.Bind(std::move(handshake_client_));
    remote_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnError,
                       base::Unretained(this), net::ERR_ABORTED));
    std::move(create_callback_)
        .Run(receiver_.BindNewPipeAndPassRemote(), absl::nullopt);
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnError,
                       base::Unretained(this), net::ERR_ABORTED));
  }

  // WebTransportHandshakeClient implementation:
  // Proxing should be finished with either of below functions.
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebTransport> transport,
      mojo::PendingReceiver<network::mojom::WebTransportClient> client,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers)
      override {
    receiver_.reset();
    pending_transport_ = std::move(transport);
    pending_client_ = std::move(client);
    response_headers_ = response_headers;
    // Since WebTransport doesn't support redirect, 'redirect_url' is ignored
    // even if extensions assigned it.
    const int result = request_handler_->OnHeadersReceived(
        browser_context_, &info_,
        base::BindOnce(&WebTransportHandshakeProxy::OnHeadersReceivedCompleted,
                       base::Unretained(this)),
        response_headers_.get(), &override_headers_, &redirect_url_);

    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    OnHeadersReceivedCompleted(result);
  }

  void OnHeadersReceivedCompleted(int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      return;
    }

    network::mojom::URLResponseHead response;
    response.headers =
        override_headers_ ? override_headers_ : response_headers_;
    DCHECK(response.headers);
    // TODO(1250090): Assign actual server IP 'response';
    response.remote_endpoint = net::IPEndPoint();
    // Web transport doesn't use the http cache.
    response.was_fetched_via_cache = false;
    info_.AddResponse(response);

    request_handler_->OnResponseStarted(browser_context_, &info_, net::OK);

    remote_->OnConnectionEstablished(std::move(pending_transport_),
                                     std::move(pending_client_),
                                     response.headers);

    OnCompleted();
    // `this` is deleted.
  }

  void OnHandshakeFailed(
      const absl::optional<net::WebTransportError>& error) override {
    remote_->OnHandshakeFailed(error);

    int error_code = net::ERR_ABORTED;
    if (error.has_value()) {
      error_code = error->net_error;
    }
    OnError(error_code);
    // `this` is deleted.
  }

  void OnError(int error_code) {
    DCHECK_NE(error_code, net::OK);
    if (create_callback_) {
      auto webtransport_error = network::mojom::WebTransportError::New(
          error_code, quic::QUIC_INTERNAL_ERROR, "Blocked by a request filter",
          false);
      std::move(create_callback_)
          .Run(std::move(handshake_client_), std::move(webtransport_error));
    }
    request_handler_->OnErrorOccurred(browser_context_, &info_,
                                      /*started=*/true, error_code);

    proxies_->RemoveProxy(this);
    // `this` is deleted.
  }

  void OnCompleted() {
    request_handler_->OnCompleted(browser_context_, &info_, net::OK);
    // Delete `this`.
    proxies_->RemoveProxy(this);
  }

 private:
  mojo::PendingRemote<WebTransportHandshakeClient> handshake_client_;
  const raw_ptr<RequestFilterManager::RequestHandler> request_handler_;
  // Weak reference to the ProxySet. This is safe as `proxies_` owns this
  // object.
  const raw_ref<RequestFilterManager::ProxySet> proxies_;
  raw_ptr<content::BrowserContext> browser_context_;
  FilteredRequestInfo info_;
  net::HttpRequestHeaders request_headers_;
  GURL redirect_url_;
  mojo::Remote<WebTransportHandshakeClient> remote_;
  mojo::Receiver<WebTransportHandshakeClient> receiver_{this};
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  scoped_refptr<net::HttpResponseHeaders> override_headers_;
  mojo::PendingRemote<network::mojom::WebTransport> pending_transport_;
  mojo::PendingReceiver<network::mojom::WebTransportClient> pending_client_;

  CreateCallback create_callback_;
};

}  // namespace

void StartWebRequestProxyingWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<WebTransportHandshakeClient> handshake_client,
    int64_t request_id,
    RequestFilterManager::RequestHandler* request_handler,
    RequestFilterManager::ProxySet& proxies,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback) {
  content::BrowserContext* browser_context =
      render_process_host.GetBrowserContext();
  // Filling ResourceRequest fields required to create WebRequestInfoInitParams.
  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kConnectMethod;
  request.url = url;
  request.request_initiator = initiator_origin;

  const int process_id = render_process_host.GetID();
  FilteredRequestInfo params = FilteredRequestInfo(
      request_id, process_id, frame_routing_id, request,
      content::ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      /*is_async=*/true, /*is_webtransport=*/true,
      /*navigation_id=*/absl::nullopt);

  auto proxy = std::make_unique<WebTransportHandshakeProxy>(
      std::move(handshake_client), request_handler, proxies, browser_context,
      std::move(params), std::move(callback));
  auto* raw_proxy = proxy.get();
  proxies.AddProxy(std::move(proxy));
  raw_proxy->Start();
}

}  // namespace vivaldi
