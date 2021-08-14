// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller_base_test.h"
#include "chrome/browser/ash/login/user_online_signin_notifier.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Mock implementation of UserOnlineSigninNotifier::Observer.
class MockUserOnlineSigninNotifierObserver
    : public UserOnlineSigninNotifier::Observer {
 public:
  MOCK_METHOD(void, OnOnlineSigninEnforced, (const AccountId& account_id));
};

constexpr base::TimeDelta kLoginOnlineShortDelay =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kLoginOnlineLongDelay =
    base::TimeDelta::FromSeconds(100);
constexpr base::TimeDelta kLoginOnlineVeryLongDelay =
    base::TimeDelta::FromSeconds(1000);
constexpr base::TimeDelta kLoginOnlineOffset = base::TimeDelta::FromSeconds(1);

}  // namespace

class UserOnlineSigninNotifierTest : public ExistingUserControllerBaseTest {
 public:
  void SetUp() override {
    mock_online_signin_notifier_observer_ =
        std::make_unique<MockUserOnlineSigninNotifierObserver>();
  }
  UserOnlineSigninNotifier* user_online_signin_notifier() const {
    return user_online_signin_notifier_.get();
  }

  // UserOnlineSigninNotifier private member accessors.
  base::OneShotTimer* online_login_refresh_timer() {
    return user_online_signin_notifier_->online_login_refresh_timer_.get();
  }

 protected:
  std::unique_ptr<MockUserOnlineSigninNotifierObserver>
      mock_online_signin_notifier_observer_;
  std::unique_ptr<UserOnlineSigninNotifier> user_online_signin_notifier_;
};

// Tests login screen update when SAMLOfflineSigninTimeLimit policy is set.
TEST_F(UserOnlineSigninNotifierTest, SamlOnlineAuthSingleUser) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account1_id_))
      .Times(1);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  // Check timer again 1s after its expected expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
}

// Verfies that `OfflineSigninLimiter` does affect SAML and non SAML user.
TEST_F(UserOnlineSigninNotifierTest, OfflineLimiteOutOfSessionSAMLAndNonSAML) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(saml_login_account1_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account1_id_))
      .Times(0);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  // Expect true since user has a value set in `kOfflineSigninLimit`.
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
}

// Tests login screen update functionality for 2 SAML users.
TEST_F(UserOnlineSigninNotifierTest, SamlOnlineAuthTwoSamlUsers) {
  base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineLongDelay);

  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account2_id_,
                                                  kLoginOnlineVeryLongDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account1_id_))
      .Times(1);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  // The timer should be re-started after
  // (kLoginOnlineLongDelay - kLoginOnlineShortDelay) s.
  task_environment_.FastForwardBy(kLoginOnlineLongDelay -
                                  kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
}

// Tests login screen update functionality for 2 users: SAML and non-SAML.
TEST_F(UserOnlineSigninNotifierTest, SamlOnlineAuthSamlAndNonSamlUsers) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetLastOnlineSignin(saml_login_account2_id_, now);

  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account2_id_,
                                                  kLoginOnlineLongDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddUser(saml_login_account2_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account1_id_))
      .Times(1);
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account2_id_))
      .Times(0);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
}

// Tests unset policy value in local state.
TEST_F(UserOnlineSigninNotifierTest, SamlOnlineAuthSamlPolicyNotSet) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  base::nullopt);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(saml_login_account1_id_))
      .Times(0);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
}

// Tests login screen update when `GaiaOfflineSigninTimeLimitDays` policy is set
// and the last online sign in has not been set. This is the case for those
// devices that went through the online signin in the first login before the
// introduction of `GaiaOfflineSigninTimeLimitDays` policy logic which didn't
// store the last online sign in.
TEST_F(UserOnlineSigninNotifierTest,
       GaiaOnlineAuthSingleUserNoLastOnlineSignin) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(1);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  // Since `LastOnlinesignin` value is null and there is a limit, it will
  // enforce the next login to be online. No timer should be running.
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
  // User logged in online after enforcement.
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(1);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  // Check timer again 1s after its expacted expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
}

// Tests login screen update when `GaiaOfflineSigninTimeLimitDays` policy is set
// and the last online sign in has been set.
TEST_F(UserOnlineSigninNotifierTest, GaiaOnlineAuthSingleUserLastOnlineSignin) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(1);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  // Check timer again 1s after its expacted expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
}

// Tests login screen update functionality for 2 Gaia without SAML users.
TEST_F(UserOnlineSigninNotifierTest, GaiaOnlineAuthTwoGaiaUsers) {
  base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineLongDelay);

  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account2_id_,
                                                  kLoginOnlineVeryLongDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  mock_user_manager()->AddUser(gaia_login_account2_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(1);
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account2_id_))
      .Times(0);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
  // The timer should be re-started after
  // (kLoginOnlineLongDelay - kLoginOnlineShortDelay) s.
  task_environment_.FastForwardBy(kLoginOnlineLongDelay -
                                  kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(online_login_refresh_timer()->IsRunning());
}

// Tests unset `GaiaOfflineTimeLimitDays` policy value in local state.
TEST_F(UserOnlineSigninNotifierTest, GaiaOnlineAuthGaiaPolicyNotSet) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  // No `LastOnlineSignin` value, case where devices didn't store that value in
  // the first Gaia login.
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  base::nullopt);

  // Case where the user has already stored last online signin.
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account2_id_,
                                                  base::nullopt);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  mock_user_manager()->AddUser(gaia_login_account2_id_);
  user_online_signin_notifier_ = std::make_unique<UserOnlineSigninNotifier>(
      mock_user_manager()->GetUsers());
  user_online_signin_notifier_->AddObserver(
      mock_online_signin_notifier_observer_.get());
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(0);
  EXPECT_CALL(*mock_online_signin_notifier_observer_,
              OnOnlineSigninEnforced(gaia_login_account1_id_))
      .Times(0);
  user_online_signin_notifier()->CheckForPolicyEnforcedOnlineSignin();
  EXPECT_FALSE(online_login_refresh_timer()->IsRunning());
}

}  // namespace chromeos
