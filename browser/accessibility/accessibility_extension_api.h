// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_

#include <string>

#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_function.h"

// API function that enables or disables web content accessibility support.
class AccessibilityPrivateSetNativeAccessibilityEnabledFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeAccessibilityEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeAccessibilityEnabled",
      ACCESSIBILITY_PRIVATE_SETNATIVEACCESSIBILITYENABLED)
};

// API function that sets the location of the accessibility focus ring.
class AccessibilityPrivateSetFocusRingsFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetFocusRingsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setFocusRings",
                             ACCESSIBILITY_PRIVATE_SETFOCUSRING)
};

// API function that sets the location of the accessibility highlights.
class AccessibilityPrivateSetHighlightsFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetHighlightsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setHighlights",
                             ACCESSIBILITY_PRIVATE_SETHIGHLIGHTS)
};

// API function that sets keyboard capture mode.
class AccessibilityPrivateSetKeyboardListenerFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetKeyboardListenerFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setKeyboardListener",
                             ACCESSIBILITY_PRIVATE_SETKEYBOARDLISTENER)
};

// API function that darkens or undarkens the screen.
class AccessibilityPrivateDarkenScreenFunction : public ExtensionFunction {
  ~AccessibilityPrivateDarkenScreenFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.darkenScreen",
                             ACCESSIBILITY_PRIVATE_DARKENSCREEN)
};

// Opens a specified subpage in Chrome settings.
class AccessibilityPrivateOpenSettingsSubpageFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateOpenSettingsSubpageFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.openSettingsSubpage",
                             ACCESSIBILITY_PRIVATE_OPENSETTINGSSUBPAGE)
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// API function that sets native ChromeVox ARC support.
class AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction()
      override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp",
      ACCESSIBILITY_PRIVATE_SETNATIVECHROMEVOXARCSUPPORTFORCURRENTAPP)
};

// API function that injects key events.
class AccessibilityPrivateSendSyntheticKeyEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticKeyEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticKeyEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICKEYEVENT)
};

// API function that enables or disables mouse events in ChromeVox.
class AccessibilityPrivateEnableMouseEventsFunction : public ExtensionFunction {
  ~AccessibilityPrivateEnableMouseEventsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.enableMouseEvents",
                             ACCESSIBILITY_PRIVATE_ENABLEMOUSEEVENTS)
};

// API function that injects mouse events.
class AccessibilityPrivateSendSyntheticMouseEventFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSendSyntheticMouseEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticMouseEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICMOUSEEVENT)
};

// API function that is called when the Select-to-Speak extension state changes.
class AccessibilityPrivateSetSelectToSpeakStateFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetSelectToSpeakStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSelectToSpeakState",
                             ACCESSIBILITY_PRIVATE_SETSELECTTOSPEAKSTATE)
};

// API function that is called when the Accessibility Common extension finds
// scrollable bounds.
class AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.handleScrollableBoundsForPointFound",
      ACCESSIBILITY_PRIVATE_HANDLESCROLLABLEBOUNDSFORPOINTFOUND)
};

// API function that is called by the Accessibility Common extension to center
// the magnifier viewport on a passed-in rect.
class AccessibilityPrivateMoveMagnifierToRectFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateMoveMagnifierToRectFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.moveMagnifierToRect",
                             ACCESSIBILITY_PRIVATE_MOVEMAGNIFIERTORECT)
};

// API function that is called when a user toggles Dictation from another
// acessibility feature.
class AccessibilityPrivateToggleDictationFunction : public ExtensionFunction {
  ~AccessibilityPrivateToggleDictationFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.toggleDictation",
                             ACCESSIBILITY_PRIVATE_TOGGLEDICTATION)
};

// API function that requests that key events be forwarded to the Switch
// Access extension.
class AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.forwardKeyEventsToSwitchAccess",
      ACCESSIBILITY_PRIVATE_FORWARDKEYEVENTSTOSWITCHACCESS)
};

// API function that is called to show or hide one of the Switch Access bubbles.
class AccessibilityPrivateUpdateSwitchAccessBubbleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateSwitchAccessBubbleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateSwitchAccessBubble",
                             ACCESSIBILITY_PRIVATE_UPDATESWITCHACCESSBUBBLE)
};

// API function that is called to start or end point scanning of the
// Switch Access extension.
class AccessibilityPrivateSetPointScanStateFunction : public ExtensionFunction {
  ~AccessibilityPrivateSetPointScanStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setPointScanState",
                             ACCESSIBILITY_PRIVATE_SETPOINTSCANSTATE)
};

// API function that is called to get the device's battery status as a string.
class AccessibilityPrivateGetBatteryDescriptionFunction
    : public ExtensionFunction {
 public:
  AccessibilityPrivateGetBatteryDescriptionFunction();
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getBatteryDescription",
                             ACCESSIBILITY_PRIVATE_GETBATTERYDESCRIPTION)

 private:
  ~AccessibilityPrivateGetBatteryDescriptionFunction() override;
};

// API function that opens or closes the virtual keyboard.
class AccessibilityPrivateSetVirtualKeyboardVisibleFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateSetVirtualKeyboardVisibleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setVirtualKeyboardVisible",
                             ACCESSIBILITY_PRIVATE_SETVIRTUALKEYBOARDVISIBLE)
};

// API function that performs an accelerator action.
class AccessibilityPrivatePerformAcceleratorActionFunction
    : public ExtensionFunction {
  ~AccessibilityPrivatePerformAcceleratorActionFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.performAcceleratorAction",
                             ACCESSIBILITY_PRIVATE_PERFORMACCELERATORACTION)
};

// API function that determines if an accessibility feature is enabled.
class AccessibilityPrivateIsFeatureEnabledFunction : public ExtensionFunction {
  ~AccessibilityPrivateIsFeatureEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.isFeatureEnabled",
                             ACCESSIBILITY_PRIVATE_ISFEATUREENABLED)
};

// API function that updates properties of the Select-to-speak panel.
class AccessibilityPrivateUpdateSelectToSpeakPanelFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateUpdateSelectToSpeakPanelFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.updateSelectToSpeakPanel",
                             ACCESSIBILITY_PRIVATE_UPDATESELECTTOSPEAKPANEL)
};

// API function that shows a confirmation dialog, with callbacks for
// confirm/cancel.
class AccessibilityPrivateShowConfirmationDialogFunction
    : public ExtensionFunction {
  ~AccessibilityPrivateShowConfirmationDialogFunction() override = default;
  ResponseAction Run() override;
  void OnDialogResult(bool confirmed);
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.showConfirmationDialog",
                             ACCESSIBILITY_PRIVATE_SHOWCONFIRMATIONDIALOG)
};
#endif  // defined (OS_CHROMEOS)

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
