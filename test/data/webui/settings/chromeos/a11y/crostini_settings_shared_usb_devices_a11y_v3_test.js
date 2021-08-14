// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the CROSTINI_SHARED_USB_DEVICES
 * route.
 * Chrome OS only.
 */

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);
flush();
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
