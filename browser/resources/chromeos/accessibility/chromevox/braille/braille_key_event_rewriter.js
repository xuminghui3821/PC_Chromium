// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Rewrites a braille key event.
 */

goog.provide('BrailleKeyEventRewriter');

goog.require('Output');
goog.require('BrailleKeyEvent');

/**
 * A class that transforms a sequence of braille key events into a standard key
 * event.
 */
BrailleKeyEventRewriter = class {
  constructor() {
    /** @private {Object} */
    this.incrementalKey_ = null;
  }

  /**
   * Accumulates and optionally modifies in-coming braille key events.
   * @param {BrailleKeyEvent} evt
   * @return {boolean} False to continue event propagation.
   */
  onBrailleKeyEvent(evt) {
    let standardKeyCode;
    const dots = evt.brailleDots;
    if (!dots) {
      this.incrementalKey_ = null;
      return false;
    }

    if (evt.command === BrailleKeyCommand.CHORD) {
      Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);
      const modifiers = BrailleKeyEvent.brailleDotsToModifiers[dots];

      // Check for a modifier mapping.
      if (modifiers) {
        this.incrementalKey_ = this.incrementalKey_ || {};
        for (const key in modifiers) {
          this.incrementalKey_[key] = true;
        }

        return true;
      }

      // Check for a chord to standard key mapping.
      standardKeyCode = BrailleKeyEvent.brailleChordsToStandardKeyCode[dots];
    }

    // Check for a 'dots' command, which is typed on the keyboard with a
    // previous incremental key press.
    if (evt.command === BrailleKeyCommand.DOTS && this.incrementalKey_) {
      // Check if this braille pattern has a standard key mapping.
      standardKeyCode = BrailleKeyEvent.brailleDotsToStandardKeyCode[dots];
    }

    if (standardKeyCode) {
      evt.command = BrailleKeyCommand.STANDARD_KEY;
      evt.standardKeyCode = standardKeyCode;
      if (this.incrementalKey_) {
        // Apply all modifiers seen so far to the outgoing event as a standard
        // keyboard command.
        evt.altKey = this.incrementalKey_.altKey;
        evt.ctrlKey = this.incrementalKey_.ctrlKey;
        evt.shiftKey = this.incrementalKey_.shiftKey;
        this.incrementalKey_ = null;
      }
      return false;
    }

    this.incrementalKey_ = null;
    return false;
  }
};
