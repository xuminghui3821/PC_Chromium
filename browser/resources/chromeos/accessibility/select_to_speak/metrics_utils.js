// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsManager} from './prefs_manager.js';

// Utilities for UMA metrics.

export class MetricsUtils {
  constructor() {}

  /**
   * Records a cancel event if speech was in progress.
   */
  static recordCancelIfSpeaking() {
    // TODO(b/1157214): Use select-to-speak's internal state instead of TTS
    // state.
    chrome.tts.isSpeaking((speaking) => {
      if (speaking) {
        MetricsUtils.recordCancelEvent_();
      }
    });
  }

  /**
   * Records an event that Select-to-Speak has begun speaking.
   * @param {number} method The CrosSelectToSpeakStartSpeechMethod enum
   *    that reflects how this event was triggered by the user.
   * @param {PrefsManager} prefsManager A PrefsManager with the users's current
   *    preferences.
   */
  static recordStartEvent(method, prefsManager) {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.START_SPEECH_METRIC);
    chrome.metricsPrivate.recordEnumerationValue(
        MetricsUtils.START_SPEECH_METHOD_METRIC.METRIC_NAME, method,
        MetricsUtils.START_SPEECH_METHOD_METRIC.EVENT_COUNT);
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.BACKGROUND_SHADING_METRIC,
        prefsManager.backgroundShadingEnabled());
    chrome.metricsPrivate.recordBoolean(
        MetricsUtils.NAVIGATION_CONTROLS_METRIC,
        prefsManager.navigationControlsEnabled());
  }

  /**
   * Records an event that Select-to-Speak speech has been canceled.
   * @private
   */
  static recordCancelEvent_() {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.CANCEL_SPEECH_METRIC);
  }

  /**
   * Records an event that Select-to-Speak speech has been paused.
   */
  static recordPauseEvent() {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.PAUSE_SPEECH_METRIC);
  }

  /**
   * Records an event that Select-to-Speak speech has been resumed from pause.
   */
  static recordResumeEvent() {
    chrome.metricsPrivate.recordUserAction(MetricsUtils.RESUME_SPEECH_METRIC);
  }

  /**
   * Records a user-requested state change event from a given state.
   * @param {number} changeType
   */
  static recordSelectToSpeakStateChangeEvent(changeType) {
    chrome.metricsPrivate.recordEnumerationValue(
        MetricsUtils.STATE_CHANGE_METRIC.METRIC_NAME, changeType,
        MetricsUtils.STATE_CHANGE_METRIC.EVENT_COUNT);
  }

  /**
   * Converts the speech multiplier into an enum based on
   * tools/metrics/histograms/enums.xml.
   * The value returned by this function is persisted to logs. Log entries
   * should not be renumbered and numeric values should never be reused, so this
   * function should not be changed.
   * @param {number} speechRate The current speech rate.
   * @return {number} The current speech rate as an int for metrics.
   * @private
   */
  static speechMultiplierToSparseHistogramInt_(speechRate) {
    return Math.floor(speechRate * 100);
  }

  /**
   * Records the speed override chosen by the user.
   * @param {number} rate
   */
  static recordSpeechRateOverrideMultiplier(rate) {
    chrome.metricsPrivate.recordSparseValue(
        MetricsUtils.OVERRIDE_SPEECH_RATE_MULTIPLIER_METRIC,
        MetricsUtils.speechMultiplierToSparseHistogramInt_(rate));
  }
}

/**
 * Defines an enumeration metric. The |EVENT_COUNT| must be kept in sync
 * with the number of enum values for each metric in
 * tools/metrics/histograms/enums.xml.
 * @typedef {{EVENT_COUNT: number, METRIC_NAME: string}}
 */
MetricsUtils.EnumerationMetric;

/**
 * CrosSelectToSpeakStartSpeechMethod enums.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
MetricsUtils.StartSpeechMethod = {
  MOUSE: 0,
  KEYSTROKE: 1,
};

/**
 * Constants for the start speech method metric,
 * CrosSelectToSpeakStartSpeechMethod.
 * @type {MetricsUtils.EnumerationMetric}
 */
MetricsUtils.START_SPEECH_METHOD_METRIC = {
  EVENT_COUNT: Object.keys(MetricsUtils.StartSpeechMethod).length,
  METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StartSpeechMethod'
};

/**
 * CrosSelectToSpeakStateChangeEvent enums.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
MetricsUtils.StateChangeEvent = {
  START_SELECTION: 0,
  CANCEL_SPEECH: 1,
  CANCEL_SELECTION: 2,
};

/**
 * Constants for the state change metric, CrosSelectToSpeakStateChangeEvent.
 * @type {MetricsUtils.EnumerationMetric}
 */
MetricsUtils.STATE_CHANGE_METRIC = {
  EVENT_COUNT: Object.keys(MetricsUtils.StateChangeEvent).length,
  METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StateChangeEvent'
};

/**
 * The start speech metric name.
 * @type {string}
 */
MetricsUtils.START_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.StartSpeech';

/**
 * The cancel speech metric name.
 * @type {string}
 */
MetricsUtils.CANCEL_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.CancelSpeech';

/**
 * The pause speech metric name.
 * @type {string}
 */
MetricsUtils.PAUSE_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.PauseSpeech';

/**
 * The resume speech after pausing metric name.
 * @type {string}
 */
MetricsUtils.RESUME_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.ResumeSpeech';

/**
 * The background shading metric name.
 * @type {string}
 */
MetricsUtils.BACKGROUND_SHADING_METRIC =
    'Accessibility.CrosSelectToSpeak.BackgroundShading';

/**
 * The navigation controls metric name.
 * @type {string}
 */
MetricsUtils.NAVIGATION_CONTROLS_METRIC =
    'Accessibility.CrosSelectToSpeak.NavigationControls';

/**
 * The speech rate override histogram metric name.
 * @type {string}
 */
MetricsUtils.OVERRIDE_SPEECH_RATE_MULTIPLIER_METRIC =
    'Accessibility.CrosSelectToSpeak.OverrideSpeechRateMultiplier';
