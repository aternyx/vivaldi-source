// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests blocking fetch in Service Workers.\n`);
  await TestRunner.loadLegacyModule('console');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('network');

  let scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/network-fetch-worker-blocked-scope';

  NetworkTestRunner.recordNetwork();
  SDK.multitargetNetworkManager.setBlockingEnabled(true);
  SDK.multitargetNetworkManager.setBlockedPatterns([{url: 'resources/resource.php', enabled: true}]);

  ApplicationTestRunner.makeFetchInServiceWorker(scope, '../../network/resources/resource.php', {}, fetchCallback);

  function fetchCallback(result) {
    TestRunner.addResult('Fetch in worker result: ' + result);

    const requests = NetworkTestRunner.networkRequests();
    requests.forEach((request) => {
      TestRunner.addResult(request.url());
      TestRunner.addResult('resource.type: ' + request.resourceType());
      TestRunner.addResult('request.failed: ' + !!request.failed);
    });

    TestRunner.completeTest();
  }
})();
