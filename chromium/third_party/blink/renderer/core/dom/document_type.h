/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_TYPE_H_

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class DocumentType final : public Node {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DocumentType(Document*,
               const String& name,
               const String& public_id,
               const String& system_id);

  const String& name() const { return name_; }
  const String& publicId() const { return public_id_; }
  const String& systemId() const { return system_id_; }

 private:
  String nodeName() const override;
  Node* Clone(Document& factory,
              NodeCloningData& data,
              ContainerNode* append_to,
              ExceptionState& append_exception_state) const override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void DetachLayoutTree(bool performing_reattach) final {}

  String name_;
  String public_id_;
  String system_id_;
};

template <>
struct DowncastTraits<DocumentType> {
  static bool AllowFrom(const Node& node) { return node.IsDocumentTypeNode(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_TYPE_H_
