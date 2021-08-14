// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('Multidevice', function() {
  let localizedLink = null;

  setup(function() {
    PolymerTest.clearBody();
    localizedLink =
        document.createElement('settings-multidevice-wifi-sync-disabled-link');
    document.body.appendChild(localizedLink);
    Polymer.dom.flush();
  });

  teardown(function() {
    localizedLink.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Contains 2 links with aria-labels', async () => {
    const chromeSyncLink = localizedLink.$$('#chromeSyncLink');
    assertTrue(!!chromeSyncLink);
    assertTrue(chromeSyncLink.hasAttribute('aria-label'));
    const learnMoreLink = localizedLink.$$('#learnMoreLink');
    assertTrue(!!learnMoreLink);
    assertTrue(learnMoreLink.hasAttribute('aria-label'));
  });

  test('Spans are aria-hidden', async () => {
    const spans = localizedLink.shadowRoot.querySelectorAll('span');
    spans.forEach((span) => {
      assertTrue(span.hasAttribute('aria-hidden'));
    });
  });

  test('ChromeSyncLink navigates to appropriate route', async () => {
    const chromeSyncLink = localizedLink.$$('#chromeSyncLink');
    chromeSyncLink.click();
    Polymer.dom.flush();

    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.OS_SYNC);
    } else {
      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.SYNC_ADVANCED);
    }
  });
});