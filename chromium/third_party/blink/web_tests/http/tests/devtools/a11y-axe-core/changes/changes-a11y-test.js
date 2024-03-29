// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

(async function() {
  TestRunner.addResult('Tests accessibility in the Changes drawer.');
  await TestRunner.loadLegacyModule('changes');

  const diff = [
    {0: Diff.Diff.Operation.Insert, 1: ['line 1 inserted']},
    {0: Diff.Diff.Operation.Delete, 1: ['line 2 deleted']},
  ];
  const uiSourceCodeMock = {mimeType: () => {}};

  TestRunner.addResult('Showing the Changes drawer.');
  await UI.viewManager.showView('changes.changes');
  const changesWidget = await UI.viewManager.view('changes.changes').widget();
  changesWidget.selectedUISourceCode = uiSourceCodeMock;
  changesWidget.renderDiffRows(diff);

  TestRunner.addResult('Running aXe on the Changes drawer.');
  await AxeCoreTestRunner.runValidation(changesWidget.contentElement);

  TestRunner.completeTest();
})();
