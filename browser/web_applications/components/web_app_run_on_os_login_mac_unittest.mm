// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <sys/xattr.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web_app {

namespace {

constexpr char kFakeChromeBundleId[] = {"fake.cfbundleidentifier"};
constexpr char kAppTitle[] = {"app"};

class WebAppAutoLoginUtilMock : public WebAppAutoLoginUtil {
 public:
  WebAppAutoLoginUtilMock() = default;
  WebAppAutoLoginUtilMock(const WebAppAutoLoginUtilMock&) = delete;
  WebAppAutoLoginUtilMock& operator=(const WebAppAutoLoginUtilMock&) = delete;

  void AddToLoginItems(const base::FilePath& app_bundle_path,
                       bool hide_on_startup) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    EXPECT_FALSE(hide_on_startup);
    add_to_login_items_called_count_++;
  }

  void RemoveFromLoginItems(const base::FilePath& app_bundle_path) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    remove_from_login_items_called_count_++;
  }

  void ResetCounts() {
    add_to_login_items_called_count_ = 0;
    remove_from_login_items_called_count_ = 0;
  }

  int GetAddToLoginItemsCalledCount() const {
    return add_to_login_items_called_count_;
  }

  int GetRemoveFromLoginItemsCalledCount() const {
    return remove_from_login_items_called_count_;
  }

 private:
  int add_to_login_items_called_count_ = 0;
  int remove_from_login_items_called_count_ = 0;
};

}  // namespace

class WebAppRunOnOsLoginMacTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    base::mac::SetBaseBundleID(kFakeChromeBundleId);

    EXPECT_TRUE(temp_destination_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    destination_dir_ = temp_destination_dir_.GetPath();
    user_data_dir_ = temp_user_data_dir_.GetPath();
    // Recreate the directory structure as it would be created for the
    // ShortcutInfo created in the above GetShortcutInfo.
    app_data_dir_ = user_data_dir_.Append("Profile 1")
                        .Append("Web Applications")
                        .Append("_crx_app-id");
    EXPECT_TRUE(base::CreateDirectory(app_data_dir_));

    // When using base::PathService::Override, it calls
    // base::MakeAbsoluteFilePath. On Mac this prepends "/private" to the path,
    // but points to the same directory in the file system.
    EXPECT_TRUE(
        base::PathService::Override(chrome::DIR_USER_DATA, user_data_dir_));
    user_data_dir_ = base::MakeAbsoluteFilePath(user_data_dir_);
    app_data_dir_ = base::MakeAbsoluteFilePath(app_data_dir_);

    SetChromeAppsFolderForTesting(destination_dir_);

    info_ = GetShortcutInfo();
    base::FilePath shim_base_name =
        base::FilePath(base::UTF16ToUTF8(info_->title) + ".app");
    shim_path_ = destination_dir_.Append(shim_base_name);

    auto_login_util_mock_ = std::make_unique<WebAppAutoLoginUtilMock>();
    WebAppAutoLoginUtil::SetInstanceForTesting(auto_login_util_mock_.get());
  }

  void TearDown() override {
    WebAppAutoLoginUtil::SetInstanceForTesting(nullptr);
    SetChromeAppsFolderForTesting(base::FilePath());
    WebAppTest::TearDown();
  }

  std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
    std::unique_ptr<ShortcutInfo> info(new ShortcutInfo);
    info->extension_id = "app-id";
    info->title = base::UTF8ToUTF16(kAppTitle);
    info->url = GURL("http://example.com/");
    info->profile_path = user_data_dir_.Append("Profile 1");
    info->profile_name = "profile name";
    info->version_for_display = "stable 1.0";
    info->is_multi_profile = true;
    return info;
  }

 protected:
  base::ScopedTempDir temp_destination_dir_;
  base::ScopedTempDir temp_user_data_dir_;
  base::FilePath app_data_dir_;
  base::FilePath destination_dir_;
  base::FilePath user_data_dir_;

  std::unique_ptr<WebAppAutoLoginUtilMock> auto_login_util_mock_;
  std::unique_ptr<ShortcutInfo> info_;
  base::FilePath shim_path_;
};

TEST_F(WebAppRunOnOsLoginMacTest, Register) {
  auto_login_util_mock_->ResetCounts();
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_TRUE(internals::RegisterRunOnOsLogin(*info_));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_EQ(auto_login_util_mock_->GetAddToLoginItemsCalledCount(), 1);
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

TEST_F(WebAppRunOnOsLoginMacTest, Unregister) {
  auto_login_util_mock_->ResetCounts();
  EXPECT_FALSE(base::PathExists(shim_path_));
  EXPECT_TRUE(internals::RegisterRunOnOsLogin(*info_));
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_EQ(auto_login_util_mock_->GetAddToLoginItemsCalledCount(), 1);
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 0);

  auto_login_util_mock_->ResetCounts();
  EXPECT_TRUE(internals::UnregisterRunOnOsLogin(
      info_->extension_id, info_->profile_path, info_->title));
  EXPECT_EQ(auto_login_util_mock_->GetRemoveFromLoginItemsCalledCount(), 1);
  EXPECT_TRUE(base::PathExists(shim_path_));
  EXPECT_TRUE(base::DeletePathRecursively(shim_path_));
}

}  // namespace web_app