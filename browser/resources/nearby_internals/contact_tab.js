// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './contact_object.js';
import './shared_style.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NearbyContactBrowserProxy} from './nearby_contact_browser_proxy.js';
import {ContactUpdate} from './types.js';

Polymer({
  is: 'contact-tab',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],


  properties: {

    /** @private {!Array<!ContactUpdate>} */
    contactList_: {
      type: Array,
      value: [],
    },
  },

  /** @private {?NearbyContactBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = NearbyContactBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript and
   * initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.addWebUIListener(
        'contacts-updated', contact => this.onContactUpdateAdded_(contact));
    this.browserProxy_.initialize();
  },

  /**
   * Downloads contacts from the Nearby Share server.
   * @private
   */
  onDownloadContacts() {
    this.browserProxy_.downloadContacts();
  },

  /**
   * Clears list of contact messages displayed.
   * @private
   */
  onClearMessagesButtonClicked_() {
    this.contactList_ = [];
  },

  /**
   * Adds contact sent in from WebUI listener to the list of displayed contacts.
   * @param {!ContactUpdate} contact
   * @private
   */
  onContactUpdateAdded_(contact) {
    this.unshift('contactList_', contact);
  },
});
