// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_MIXIN_H_

#include <memory>
#include <utility>

#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/policy_builder.h"

namespace policy {

// Mixin to set up device and user affiliation ids. By default, device and user
// affiliation ids will be identical, and the user will be affiliated.
// `set_affiliated(false)` can be used to change this behavior.
// This mixin relies on an available `chromeos::FakeSessionManagerClient` during
// `SetUpInProcessBrowserTestFixture()`. Users of this mixin can run
// `chromeos::SessionManagerClient::InitializeFakeInMemory();` to ensure this is
// the case.
class AffiliationMixin final : public InProcessBrowserTestMixin {
 public:
  explicit AffiliationMixin(
      InProcessBrowserTestMixinHost* host,
      DevicePolicyCrosTestHelper* device_policy_cros_test_helper);
  AffiliationMixin(const AffiliationMixin&) = delete;
  AffiliationMixin& operator=(const AffiliationMixin&) = delete;
  ~AffiliationMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;

  // Returns the account id of the user.
  AccountId account_id() const { return account_id_; }

  // Sets if the user is an Active Directory user. False by default. Needs to be
  // called before SetUp to have an effect (e.g., directly after mixin
  // construction).
  void SetIsForActiveDirectory(bool is_for_active_directory);

  // Sets if the user is affiliated with the device. True by default. Needs to
  // be called before SetUp to have an effect (e.g., directly after mixin
  // construction).
  void set_affiliated(bool affiliated) { affiliated_ = affiliated; }

  // Returns the user policies of the user. Needs to be used to change the
  // user's policies. Can only be used after setup is complete.
  std::unique_ptr<UserPolicyBuilder> TakeUserPolicy() {
    return std::move(user_policy_);
  }

 private:
  AffiliationTestHelper GetAffiliationTestHelper() const;

  DevicePolicyCrosTestHelper* const policy_test_helper_;
  bool affiliated_ = true;
  bool is_for_active_directory_ = false;
  AccountId account_id_;
  std::unique_ptr<UserPolicyBuilder> user_policy_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_MIXIN_H_
