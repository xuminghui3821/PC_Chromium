// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

namespace media_router {

TEST(MediaRouterFeatureTest, GetCastAllowAllIPsPref) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(
      prefs::kMediaRouterCastAllowAllIPs, false);
  EXPECT_FALSE(GetCastAllowAllIPsPref(pref_service.get()));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCastAllowAllIPsFeature);
  EXPECT_TRUE(GetCastAllowAllIPsPref(pref_service.get()));

  pref_service->SetManagedPref(prefs::kMediaRouterCastAllowAllIPs,
                               std::make_unique<base::Value>(true));
  EXPECT_TRUE(GetCastAllowAllIPsPref(pref_service.get()));

  pref_service->SetManagedPref(prefs::kMediaRouterCastAllowAllIPs,
                               std::make_unique<base::Value>(false));
  EXPECT_FALSE(GetCastAllowAllIPsPref(pref_service.get()));
}

TEST(MediaRouterFeatureTest, GetReceiverIdHashToken) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterStringPref(
      prefs::kMediaRouterReceiverIdHashToken, "");

  std::string token = GetReceiverIdHashToken(pref_service.get());
  EXPECT_FALSE(token.empty());

  // Token stays the same on subsequent invocation.
  EXPECT_EQ(token, GetReceiverIdHashToken(pref_service.get()));
}

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
class MediaRouterEnabledTest : public ::testing::Test {
 public:
  MediaRouterEnabledTest() = default;
  MediaRouterEnabledTest(const MediaRouterEnabledTest&) = delete;
  ~MediaRouterEnabledTest() override = default;
  MediaRouterEnabledTest& operator=(const MediaRouterEnabledTest&) = delete;

 protected:
  content::BrowserTaskEnvironment test_environment;
  TestingProfile enabled_profile;
  TestingProfile disabled_profile;
};

TEST_F(MediaRouterEnabledTest, TestEnabledByPolicy) {
  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));

  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  // Runtime changes are not supported.
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));
}

TEST_F(MediaRouterEnabledTest, TestDisabledByPolicy) {
  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));

  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  // Runtime changes are not supported.
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));
}
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace media_router
