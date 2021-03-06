// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_config.h"

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updates {
namespace {

TEST(UpdateNotificationConfigTest, FinchConfigTest) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::map<std::string, std::string> parameters = {
      {kUpdateNotificationStateParamName, "true"},
      {kUpdateNotificationInitIntervalParamName, base::NumberToString(7)},
      {kUpdateNotificationMaxIntervalParamName, base::NumberToString(123)},
      {kUpdateNotificationDeliverWindowMorningStartParamName,
       base::NumberToString(5)},
      {kUpdateNotificationDeliverWindowMorningEndParamName,
       base::NumberToString(6)},
      {kUpdateNotificationDeliverWindowEveningStartParamName,
       base::NumberToString(21)},
      {kUpdateNotificationDeliverWindowEveningEndParamName,
       base::NumberToString(23)},
  };
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      chrome::android::kInlineUpdateFlow, parameters);
  std::unique_ptr<UpdateNotificationConfig> config =
      UpdateNotificationConfig::CreateFromFinch();
  EXPECT_TRUE(config->is_enabled);
  EXPECT_EQ(config->init_interval.InDays(), 7);
  EXPECT_EQ(config->max_interval.InDays(), 123);
  EXPECT_EQ(config->deliver_window_morning.first.InHours(), 5);
  EXPECT_EQ(config->deliver_window_morning.second.InHours(), 6);
  EXPECT_EQ(config->deliver_window_evening.first.InHours(), 21);
  EXPECT_EQ(config->deliver_window_evening.second.InHours(), 23);
}

}  // namespace

}  // namespace updates
