// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_vars.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="read-later-shared-style">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));