// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"

#include "base/feature_list.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_features.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace settings {
namespace features {

bool IsGuestModeActive() {
  return user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
         user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();
}

bool ShouldShowParentalControlSettings(const Profile* profile) {
  // Not shown for secondary users.
  if (profile != ProfileManager::GetPrimaryUserProfile())
    return false;

  // Also not shown for guest sessions.
  if (profile->IsGuestSession())
    return false;

  return profile->IsChild() ||
         !profile->GetProfilePolicyConnector()->IsManaged();
}

bool ShouldShowExternalStorageSettings(const Profile* profile) {
  return base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature) &&
         arc::IsArcPlayStoreEnabledForProfile(profile);
}

}  // namespace features
}  // namespace settings
}  // namespace chromeos
