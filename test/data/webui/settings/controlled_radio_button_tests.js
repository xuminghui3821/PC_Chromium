// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('controlled radio button', function() {
  /** @type {ControlledRadioButtonElement} */
  let radioButton;

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const pref = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true
  };

  setup(function() {
    PolymerTest.clearBody();
    radioButton = document.createElement('controlled-radio-button');
    radioButton.set('pref', pref);
    document.body.appendChild(radioButton);
  });

  test('disables when pref is managed', function() {
    radioButton.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    flush();
    assertTrue(radioButton.disabled);
    assertFalse(!!radioButton.$$('cr-policy-pref-indicator'));

    radioButton.set('name', 'true');
    flush();
    assertTrue(!!radioButton.$$('cr-policy-pref-indicator'));

    // See https://github.com/Polymer/polymer/issues/4652#issuecomment-305471987
    // on why |null| must be used here instead of |undefined|.
    radioButton.set('pref.enforcement', null);
    flush();
    assertFalse(radioButton.disabled);
    assertEquals(
        'none', radioButton.$$('cr-policy-pref-indicator').style.display);
  });
});
