// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_

#include "ash/public/cpp/accelerators.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

using ::extensions::api::braille_display_private::StubBrailleController;

// Spoken feedback tests only in a logged in user's window.
class LoggedInSpokenFeedbackTest : public InProcessBrowserTest {
 public:
  LoggedInSpokenFeedbackTest();
  ~LoggedInSpokenFeedbackTest() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;

  // Simulate key press event.
  void SendKeyPress(ui::KeyboardCode key);
  void SendKeyPressWithControl(ui::KeyboardCode key);
  void SendKeyPressWithShift(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndShift(ui::KeyboardCode key);
  void SendKeyPressWithSearch(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndControl(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndControlAndShift(ui::KeyboardCode key);

  void SendStickyKeyCommand();

  void SendMouseMoveTo(const gfx::Point& location);

  bool PerformAcceleratorAction(AcceleratorAction action);

  void DisableEarcons();

  void EnableChromeVox();

  void StablizeChromeVoxState();

  void PressRepeatedlyUntilUtterance(ui::KeyboardCode key,
                                     const std::string& expected_utterance);

  test::SpeechMonitor sm_;

 private:
  StubBrailleController braille_controller_;
  ui::ScopedAnimationDurationScaleMode animation_mode_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;

  DISALLOW_COPY_AND_ASSIGN(LoggedInSpokenFeedbackTest);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_
