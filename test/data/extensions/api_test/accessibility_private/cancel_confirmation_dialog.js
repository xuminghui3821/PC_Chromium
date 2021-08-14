// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.accessibilityPrivate.showConfirmationDialog(
    'Cancel me!', 'This dialog should be canceled', (confirmed) => {
      if (confirmed) {
        chrome.test.fail();
      } else {
        chrome.test.succeed();
      }
    });

chrome.test.notifyPass();
