// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

constexpr SkColor kNewProfileColor = SK_ColorRED;
constexpr SkColor kSyncedProfileColor = SK_ColorBLUE;

class FakeThemeService : public ThemeService {
 public:
  explicit FakeThemeService(const ThemeHelper& theme_helper)
      : ThemeService(nullptr, theme_helper) {}

  void SetThemeSyncableService(ThemeSyncableService* theme_syncable_service) {
    theme_syncable_service_ = theme_syncable_service;
  }

  // ThemeService:
  void DoSetTheme(const extensions::Extension* extension,
                  bool suppress_infobar) override {
    using_default_theme_ = false;
    color_ = 0;
  }

  void BuildAutogeneratedThemeFromColor(SkColor color) override {
    color_ = color;
    using_default_theme_ = false;
  }

  void UseDefaultTheme() override {
    using_default_theme_ = true;
    color_ = 0;
  }

  bool UsingDefaultTheme() const override { return using_default_theme_; }

  SkColor GetAutogeneratedThemeColor() const override { return color_; }

  ThemeSyncableService* GetThemeSyncableService() const override {
    return theme_syncable_service_;
  }

 private:
  ThemeSyncableService* theme_syncable_service_ = nullptr;
  bool using_default_theme_ = true;
  SkColor color_ = 0;
};

class ProfileCustomizationBubbleSyncControllerTest : public testing::Test {
 public:
  ProfileCustomizationBubbleSyncControllerTest()
      : fake_theme_service_(theme_helper_),
        theme_syncable_service_(nullptr, &fake_theme_service_) {
    fake_theme_service_.SetThemeSyncableService(&theme_syncable_service_);
  }

  void ApplyColorAndShowBubbleWhenNoValueSynced(
      base::OnceCallback<void(bool)> show_bubble_callback) {
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSyncedForTesting(
            &test_sync_service_, &fake_theme_service_,
            std::move(show_bubble_callback), kNewProfileColor);
  }

  void SetSyncedProfileColor() {
    fake_theme_service_.BuildAutogeneratedThemeFromColor(kSyncedProfileColor);
  }

  void SetSyncedProfileTheme() {
    fake_theme_service_.DoSetTheme(nullptr, false);
  }

  void NotifyOnSyncStarted(bool waiting_for_extension_installation = false) {
    theme_syncable_service_.NotifyOnSyncStartedForTesting(
        waiting_for_extension_installation
            ? ThemeSyncableService::ThemeSyncState::
                  kWaitingForExtensionInstallation
            : ThemeSyncableService::ThemeSyncState::kApplied);
  }

 protected:
  syncer::TestSyncService test_sync_service_;
  base::HistogramTester histogram_tester_;

 private:
  FakeThemeService fake_theme_service_;
  ThemeSyncableService theme_syncable_service_;
  ThemeHelper theme_helper_;
};

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldShowWhenSyncGetsDefaultTheme) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(true));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  NotifyOnSyncStarted();
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldShowWhenSyncDisabled) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(true));

  test_sync_service_.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomColor) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(false));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  SetSyncedProfileColor();
  NotifyOnSyncStarted();
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomTheme) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(false));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  SetSyncedProfileTheme();
  NotifyOnSyncStarted();
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomThemeToInstall) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(false));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  NotifyOnSyncStarted(/*waiting_for_extension_installation=*/true);
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncHasCustomPasshrase) {
  base::MockCallback<base::OnceCallback<void(bool)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(false));

  test_sync_service_.SetPassphraseRequired(true);
  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  test_sync_service_.FireStateChanged();
  histogram_tester_.ExpectTotalCount("Profile.SyncCustomizationBubbleDelay", 1);
}

}  // namespace
