// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerToolbarDropdownElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const tests = [
  /**
   * Test that clicking the dropdown icon opens/closes the dropdown.
   */
  function testToolbarDropdownShowHide() {
    document.body.innerHTML = '';
    const dropdown = /** @type {!ViewerToolbarDropdownElement} */ (
        document.createElement('viewer-toolbar-dropdown'));
    dropdown.header = 'Test Menu';
    dropdown.closedIcon = 'closedIcon';
    dropdown.openIcon = 'openIcon';
    document.body.appendChild(dropdown);

    const button = dropdown.$.button;
    chrome.test.assertFalse(dropdown.dropdownOpen);
    chrome.test.assertEq('closedIcon,cr:arrow-drop-down', button.ironIcon);

    button.click();

    chrome.test.assertTrue(dropdown.dropdownOpen);
    chrome.test.assertEq('openIcon,cr:arrow-drop-down', button.ironIcon);

    button.click();

    chrome.test.assertFalse(dropdown.dropdownOpen);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
