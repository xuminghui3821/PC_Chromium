// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_USER_AVATAR_IMAGE_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_USER_AVATAR_IMAGE_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/policy/external_data_handlers/cloud_external_data_policy_handler.h"

namespace ash {
class CrosSettings;
}  // namespace ash

namespace policy {

class DeviceLocalAccountPolicyService;

class UserAvatarImageExternalDataHandler
    : public CloudExternalDataPolicyHandler {
 public:
  UserAvatarImageExternalDataHandler(
      ash::CrosSettings* cros_settings,
      DeviceLocalAccountPolicyService* policy_service);
  ~UserAvatarImageExternalDataHandler() override;

  // CloudExternalDataPolicyHandler:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override;
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path) override;
  void RemoveForAccountId(const AccountId& account_id) override;

 private:
  CloudExternalDataPolicyObserver user_avatar_image_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserAvatarImageExternalDataHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_USER_AVATAR_IMAGE_EXTERNAL_DATA_HANDLER_H_
