// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/edu_coexistence_tos_store_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {

namespace edu_coexistence {

const char kMinTOSVersionNumber[] = "337351677";

UserConsentInfo::UserConsentInfo(const std::string& gaia_id,
                                 const std::string& version)
    : edu_account_gaia_id(gaia_id), edu_coexistence_tos_version(version) {}

bool IsConsentVersionLessThan(const std::string& lhs_version,
                              const std::string& rhs_version) {
  uint64_t lhs_version_int;
  if (!base::StringToUint64(lhs_version, &lhs_version_int)) {
    LOG(ERROR) << " TermsOfService |lhs_version| string is not a number"
               << lhs_version;
    return false;
  }

  uint64_t rhs_version_int;
  if (!base::StringToUint64(rhs_version, &rhs_version_int)) {
    LOG(ERROR) << " TermsOfService |rhs_version| string is not a number"
               << rhs_version;
    return false;
  }

  return lhs_version_int < rhs_version_int;
}

void UpdateAcceptedToSVersionPref(Profile* profile,
                                  const UserConsentInfo& user_consent_info) {
  DictionaryPrefUpdate update(
      profile->GetPrefs(), chromeos::prefs::kEduCoexistenceToSAcceptedVersion);
  base::DictionaryValue* dict = update.Get();

  dict->SetStringPath(user_consent_info.edu_account_gaia_id,
                      user_consent_info.edu_coexistence_tos_version);
}

void SetUserConsentInfoListForProfile(
    Profile* profile,
    const std::vector<UserConsentInfo>& user_consent_info_list) {
  base::Value user_consent_info_list_value(base::Value::Type::DICTIONARY);
  for (const auto& info : user_consent_info_list) {
    user_consent_info_list_value.SetStringPath(
        info.edu_account_gaia_id, info.edu_coexistence_tos_version);
  }

  profile->GetPrefs()->Set(chromeos::prefs::kEduCoexistenceToSAcceptedVersion,
                           std::move(user_consent_info_list_value));
}

std::vector<UserConsentInfo> GetUserConsentInfoListForProfile(
    Profile* profile) {
  const base::Value* user_consent_info_dict_value = profile->GetPrefs()->Get(
      chromeos::prefs::kEduCoexistenceToSAcceptedVersion);

  DCHECK(user_consent_info_dict_value);
  DCHECK(user_consent_info_dict_value->is_dict());

  std::vector<UserConsentInfo> info_list;

  for (const auto& entry : user_consent_info_dict_value->DictItems()) {
    const std::string& gaia_id = entry.first;
    const std::string& accepted_tos_version = entry.second.GetString();
    info_list.push_back(UserConsentInfo(gaia_id, accepted_tos_version));
  }

  return info_list;
}

std::string GetAcceptedToSVersion(Profile* profile,
                                  const std::string& secondary_edu_gaia_id) {
  const base::Value* accepted_values = profile->GetPrefs()->Get(
      chromeos::prefs::kEduCoexistenceToSAcceptedVersion);

  const std::string* entry =
      accepted_values->FindStringKey(secondary_edu_gaia_id);
  return entry ? *entry : std::string();
}

}  // namespace edu_coexistence
}  // namespace chromeos
