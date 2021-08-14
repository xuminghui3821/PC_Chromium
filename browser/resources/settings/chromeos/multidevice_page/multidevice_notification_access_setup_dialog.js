// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides the Phone Hub notification access setup flow that, when
 * successfully completed, enables the feature that allows a user's phone
 * notifications to be mirrored on their Chromebook.
 */

/**
 * Numerical values should not be changed because they must stay in sync with
 * notification_access_setup_operation.h, with the exception of
 * CONNECTION_REQUESTED.
 * @enum{number}
 */
/* #export */ const NotificationAccessSetupOperationStatus = {
  CONNECTION_REQUESTED: 0,
  CONNECTING: 1,
  TIMED_OUT_CONNECTING: 2,
  CONNECTION_DISCONNECTED: 3,
  SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE: 4,
  COMPLETED_SUCCESSFULLY: 5,
  NOTIFICATION_ACCESS_PROHIBITED: 6,
};

Polymer({
  is: 'settings-multidevice-notification-access-setup-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * A null |setupState_| indicates that the operation has not yet started.
     * @private {?NotificationAccessSetupOperationStatus}
     */
    setupState_: {
      type: Number,
      value: null,
    },

    /** @private */
    title_: {
      type: String,
      computed: 'getTitle_(setupState_)',
    },

    /** @private */
    description_: {
      type: String,
      computed: 'getDescription_(setupState_)',
    },

    /** @private */
    hasNotStartedSetupAttempt_: {
      type: Boolean,
      computed: 'computeHasNotStartedSetupAttempt_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    isSetupAttemptInProgress_: {
      type: Boolean,
      computed: 'computeIsSetupAttemptInProgress_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    didSetupAttemptFail_: {
      type: Boolean,
      computed: 'computeDidSetupAttemptFail_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    hasCompletedSetupSuccessfully_: {
      type: Boolean,
      computed: 'computeHasCompletedSetupSuccessfully_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    isNotificationAccessProhibited_: {
      type: Boolean,
      computed: 'computeIsNotificationAccessProhibited_(setupState_)',
    },

    /** @private */
    shouldShowSetupInstructionsSeparately_: {
      type: Boolean,
      computed: 'computeShouldShowSetupInstructionsSeparately_(' +
          'setupState_)',
      reflectToAttribute: true,
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'settings.onNotificationAccessSetupStatusChanged',
        this.onSetupStateChanged_.bind(this));
    this.$.dialog.showModal();
  },

  /**
   * @param {!NotificationAccessSetupOperationStatus} setupState
   * @private
   */
  onSetupStateChanged_(setupState) {
    this.setupState_ = setupState;
    if (this.setupState_ ===
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY) {
      this.browserProxy_.setFeatureEnabledState(
          settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasNotStartedSetupAttempt_() {
    return this.setupState_ === null;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSetupAttemptInProgress_() {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus
            .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasCompletedSetupSuccessfully_() {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsNotificationAccessProhibited_() {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean}
   * @private
   * */
  computeDidSetupAttemptFail_() {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean} Whether to show setup instructions in its own section.
   * @private
   */
  computeShouldShowSetupInstructionsSeparately_() {
    return this.setupState_ === null ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus
            .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ === NotificationAccessSetupOperationStatus.CONNECTING;
  },

  /** @private */
  attemptNotificationSetup_() {
    this.browserProxy_.attemptNotificationSetup();
    this.setupState_ =
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED;
  },

  /** @private */
  onCancelClicked_() {
    this.browserProxy_.cancelNotificationSetup();
    this.$.dialog.close();
  },

  /** @private */
  onDoneOrCloseButtonClicked_() {
    this.$.dialog.close();
  },

  /**
   * @return {string} The title of the dialog.
   * @private
   */
  getTitle_() {
    if (this.setupState_ === null) {
      return this.i18n('multideviceNotificationAccessSetupAckTitle');
    }

    const Status = NotificationAccessSetupOperationStatus;
    switch (this.setupState_) {
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
        return this.i18n('multideviceNotificationAccessSetupConnectingTitle');
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return this.i18n(
            'multideviceNotificationAccessSetupAwaitingResponseTitle');
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multideviceNotificationAccessSetupCompletedTitle');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n(
            'multideviceNotificationAccessSetupCouldNotEstablishConnectionTitle');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n(
            'multideviceNotificationAccessSetupConnectionLostWithPhoneTitle');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18n(
            'multideviceNotificationAccessSetupAccessProhibitedTitle');
      default:
        return '';
    }
  },

  /**
   * @return {string} A description about the connection attempt state.
   * @private
   */
  getDescription_() {
    if (this.setupState_ === null) {
      return this.i18n('multideviceNotificationAccessSetupAckSummary');
    }

    const Status = NotificationAccessSetupOperationStatus;
    switch (this.setupState_) {
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multideviceNotificationAccessSetupCompletedSummary');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n(
            'multideviceNotificationAccessSetupEstablishFailureSummary');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n(
            'multideviceNotificationAccessSetupMaintainFailureSummary');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18nAdvanced(
            'multideviceNotificationAccessSetupAccessProhibitedSummary');
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return this.i18n(
            'multideviceNotificationAccessSetupAwaitingResponseSummary');

      // Only setup instructions will be shown.
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCancelButton_() {
    return this.setupState_ !==
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY &&
        this.setupState_ !==
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowTryAgainButton_() {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED;
  },
});
