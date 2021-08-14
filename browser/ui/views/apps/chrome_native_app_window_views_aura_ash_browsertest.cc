// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/apps/platform_apps/app_window_interactive_uitest_base.h"
#include "chrome/browser/ui/ash/tablet_mode_page_behavior.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/wm/core/window_util.h"

namespace {

class ViewBoundsChangeWaiter : public views::ViewObserver {
 public:
  static void VerifyY(views::View* view, int y) {
    if (y != view->bounds().y())
      ViewBoundsChangeWaiter(view).run_loop_.Run();

    EXPECT_EQ(y, view->bounds().y());
  }

 private:
  explicit ViewBoundsChangeWaiter(views::View* view) {
    observation_.Observe(view);
  }
  ~ViewBoundsChangeWaiter() override = default;

  // ViewObserver:
  void OnViewBoundsChanged(views::View* view) override { run_loop_.Quit(); }

  base::RunLoop run_loop_;

  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};

  DISALLOW_COPY_AND_ASSIGN(ViewBoundsChangeWaiter);
};

}  // namespace

class ChromeNativeAppWindowViewsAuraAshBrowserTest
    : public AppWindowInteractiveTest {
 public:
  ChromeNativeAppWindowViewsAuraAshBrowserTest() = default;
  ~ChromeNativeAppWindowViewsAuraAshBrowserTest() override = default;

 protected:
  void InitWindow() { app_window_ = CreateTestAppWindow("{}"); }

  bool IsImmersiveActive() {
    return window()->widget()->GetNativeWindow()->GetProperty(
        chromeos::kImmersiveIsActive);
  }

  ChromeNativeAppWindowViewsAuraAsh* window() {
    return static_cast<ChromeNativeAppWindowViewsAuraAsh*>(
        GetFirstAppWindow()->GetBaseWindow());
  }

  std::unique_ptr<ExtensionTestMessageListener>
  LaunchPlatformAppWithFocusedWindow() {
    std::unique_ptr<ExtensionTestMessageListener> launched_listener =
        std::make_unique<ExtensionTestMessageListener>("Launched", true);
    LoadAndLaunchPlatformApp("leave_fullscreen", launched_listener.get());

    // We start by making sure the window is actually focused.
    EXPECT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        GetFirstAppWindow()->GetNativeWindow()));
    return launched_listener;
  }

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  void WaitFullscreenChange(ExtensionTestMessageListener* launched_listener) {
    FullscreenChangeWaiter fullscreen_changed(
        GetFirstAppWindow()->GetBaseWindow());
    launched_listener->Reply("window");
    fullscreen_changed.Wait();
  }

  // Because the DOM way to go fullscreen requires user gesture, we simulate a
  // key event to get the window to enter fullscreen mode. The reply will
  // make the window listen for the key event. The reply will be sent to the
  // renderer process before the keypress and should be received in that order.
  // When receiving the key event, the application will try to go fullscreen
  // using the Window API but there is no synchronous way to know if that
  // actually succeeded. Also, failure will not be notified. A failure case will
  // only be known with a timeout.
  void WaitFullscreenChangeUntilKeyFocus(
      ExtensionTestMessageListener* launched_listener) {
    launched_listener->Reply("dom");

    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());
    WaitUntilKeyFocus();
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));
    fs_changed.Wait();
  }

  extensions::AppWindow* app_window_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAuraAshBrowserTest);
};

// Verify that immersive mode is enabled or disabled as expected.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveWorkFlow) {
  InitWindow();
  ASSERT_TRUE(window());
  EXPECT_FALSE(IsImmersiveActive());
  constexpr int kFrameHeight = 32;

  views::ClientView* client_view =
      window()->widget()->non_client_view()->client_view();
  EXPECT_EQ(kFrameHeight, client_view->bounds().y());

  // Verify that when fullscreen is toggled on, immersive mode is enabled and
  // that when fullscreen is toggled off, immersive mode is disabled.
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, 0);

  app_window_->Restore();
  EXPECT_FALSE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, kFrameHeight);

  // Verify that since the auto hide title bars in tablet mode feature turned
  // on, immersive mode is enabled once tablet mode is entered, and disabled
  // once tablet mode is exited.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, 0);

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, kFrameHeight);

  // Verify that the window was fullscreened before entering tablet mode, it
  // will remain fullscreened after exiting tablet mode.
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_TRUE(IsImmersiveActive());
  app_window_->Restore();

  // Verify that minimized windows do not have immersive mode enabled.
  app_window_->Minimize();
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(IsImmersiveActive());
  window()->Restore();
  EXPECT_TRUE(IsImmersiveActive());
  app_window_->Minimize();
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());

  // Verify that activation change should not change the immersive
  // state.
  window()->Show();
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ::wm::DeactivateWindow(window()->GetNativeWindow());
  EXPECT_TRUE(IsImmersiveActive());
  ::wm::ActivateWindow(window()->GetNativeWindow());
  EXPECT_TRUE(IsImmersiveActive());

  CloseAppWindow(app_window_);
}

// Verifies that apps in immersive fullscreen will have a restore state of
// maximized.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveModeFullscreenRestoreType) {
  InitWindow();
  ASSERT_TRUE(window());

  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());

  CloseAppWindow(app_window_);
}

// Verify that immersive mode stays disabled when entering tablet mode in
// forced fullscreen mode (e.g. when running in a kiosk session).
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveModeWhenForcedFullscreen) {
  InitWindow();
  ASSERT_TRUE(window());

  app_window_->ForcedFullscreen();

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());
}

// Verify that immersive mode stays disabled in the public session, no matter
// that the app is in a normal window or fullscreen mode.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       PublicSessionNoImmersiveModeWhenFullscreen) {
  chromeos::ScopedTestPublicSessionLoginState login_state;

  InitWindow();
  ASSERT_TRUE(window());
  EXPECT_FALSE(IsImmersiveActive());

  app_window_->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_HTML_API,
                             true);

  EXPECT_FALSE(IsImmersiveActive());
}

// Verifies that apps in clamshell mode with immersive fullscreen enabled will
// correctly exit immersive mode if exit fullscreen.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       RestoreImmersiveMode) {
  InitWindow();
  ASSERT_TRUE(window());

  // Should not disable immersive fullscreen in tablet mode if |window| exits
  // fullscreen.
  EXPECT_FALSE(window()->IsFullscreen());
  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());

  window()->Restore();
  // Restoring a window inside tablet mode should deactivate fullscreen, but not
  // disable immersive mode.
  EXPECT_FALSE(window()->IsFullscreen());
  ASSERT_TRUE(IsImmersiveActive());

  // Immersive fullscreen should be disabled if window exits fullscreen in
  // clamshell mode.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  EXPECT_TRUE(window()->IsFullscreen());

  window()->Restore();
  EXPECT_FALSE(IsImmersiveActive());

  CloseAppWindow(app_window_);
}

// Ensures that JS-activated fullscreen doesn't trigger the immersive mode or
// show a bubble except the public session. (Window API)
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveOrBubbleOutsidePublicSessionWindow) {
  std::unique_ptr<ExtensionTestMessageListener> launched_listener =
      LaunchPlatformAppWithFocusedWindow();
  WaitFullscreenChange(launched_listener.get());

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_FALSE(window()->exclusive_access_bubble_);
}

// Ensures that JS-activated fullscreen doesn't trigger the immersive mode or
// show a bubble except the public session. (DOM)
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveOrBubbleOutsidePublicSessionDom) {
  std::unique_ptr<ExtensionTestMessageListener> launched_listener =
      LaunchPlatformAppWithFocusedWindow();
  WaitFullscreenChangeUntilKeyFocus(launched_listener.get());

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_FALSE(window()->exclusive_access_bubble_);
}

// Ensures that JS-activated fullscreen in the Public session doesn't trigger
// the immersive mode, but shows a bubble to guide users how to exit the
// fullscreen mode under different conditions. (Window API)
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       BubbleInsidePublicSessionWindow) {
  chromeos::ScopedTestPublicSessionLoginState state;
  std::unique_ptr<ExtensionTestMessageListener> launched_listener =
      LaunchPlatformAppWithFocusedWindow();
  WaitFullscreenChange(launched_listener.get());

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_TRUE(window()->exclusive_access_bubble_);
}

// Ensures that JS-activated fullscreen in the Public session doesn't trigger
// the immersive mode, but shows a bubble to guide users how to exit the
// fullscreen mode under different conditions. (DOM)
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       BubbleInsidePublicSessionDom) {
  chromeos::ScopedTestPublicSessionLoginState state;
  std::unique_ptr<ExtensionTestMessageListener> launched_listener =
      LaunchPlatformAppWithFocusedWindow();
  WaitFullscreenChangeUntilKeyFocus(launched_listener.get());

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_TRUE(window()->exclusive_access_bubble_);
}

// Tests the auto positioning logic of created windows does not apply to apps
// which specify their own positions.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       UserGivenBoundsAreRespected) {
  display::DisplayManager* display_manager =
      ash::ShellTestApi().display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x800");

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");

  // This is the default size apps are if no window or content specifications
  // are given.
  const gfx::Size default_size(512, 384);

  // Create an app with no window or content specifications. Use no frame for
  // simpler calculations.
  extensions::AppWindow::CreateParams params;
  params.frame = extensions::AppWindow::FRAME_NONE;
  extensions::AppWindow* app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);

  // Test that the window is centered within the work area.
  gfx::Rect expected_bounds = display_manager->GetDisplayAt(0).work_area();
  expected_bounds.ClampToCenteredSize(default_size);
  EXPECT_EQ(expected_bounds,
            app_window->GetNativeWindow()->GetBoundsInScreen());
  CloseAppWindow(app_window);

  // Create an app with content specifications. The window is placed where the
  // user specified.
  {
    const gfx::Rect specified_bounds(10, 10, 600, 400);
    extensions::AppWindow::BoundsSpecification content_spec;
    content_spec.bounds = specified_bounds;
    params.content_spec = content_spec;
    app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    EXPECT_EQ(specified_bounds,
              app_window->GetNativeWindow()->GetBoundsInScreen());
  }
  CloseAppWindow(app_window);

  // Create an app with content specifications on the secondary display. The
  // window is placed where the user specified.
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x800,800+0-800x800");
  {
    const gfx::Rect specified_bounds(810, 10, 600, 400);
    extensions::AppWindow::BoundsSpecification content_spec;
    content_spec.bounds = specified_bounds;
    params.content_spec = content_spec;
    app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    EXPECT_EQ(specified_bounds,
              app_window->GetNativeWindow()->GetBoundsInScreen());
  }
  CloseAppWindow(app_window);
}
