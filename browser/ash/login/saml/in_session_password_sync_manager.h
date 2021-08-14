// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace user_manager {
class User;
}

namespace chromeos {
class CryptohomeAuthenticator;
class ExtendedAuthenticator;

using PasswordChangedCallback = base::RepeatingClosure;

// Manages SAML password sync for multiple customer devices. Handles online
// re-auth requests triggered by online signin policy or by checking validity
// of DM sync token and sends user through online signin flow on the lock
// screen.
// This object is created by InSessionPasswordSyncManagerFactory per primary
// user in active session only when LockScreenReauthenticationEnabled
// policy is set.
class InSessionPasswordSyncManager
    : public KeyedService,
      public session_manager::SessionManagerObserver,
      public PasswordSyncTokenFetcher::Consumer,
      public AuthStatusConsumer {
 public:
  enum class ReauthenticationReason {
    kNone,
    // Enforced by the timeout set in SAMLOfflineSigninTimeLimit policy.
    kPolicy,
    // Enforced by mismatch between sync token API endpoint and the local copy
    // of the token.
    kInvalidToken
  };

  explicit InSessionPasswordSyncManager(Profile* primary_profile);
  ~InSessionPasswordSyncManager() override;

  InSessionPasswordSyncManager(const InSessionPasswordSyncManager&) = delete;
  InSessionPasswordSyncManager& operator=(const InSessionPasswordSyncManager&) =
      delete;

  // Checks if lockscreen re-authentication is enabled for the given profile.
  // Note that it can be changed in session. Driven by the policy
  // SamlLockScreenReauthenticationEnabled.
  bool IsLockReauthEnabled();

  // Sets online re-auth on lock flag and changes the UI to online
  // re-auth when called on the lock screen.
  void MaybeForceReauthOnLockScreen(ReauthenticationReason reauth_reason);

  // Set special clock for testing.
  void SetClockForTesting(const base::Clock* clock);

  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver::
  void OnSessionStateChanged() override;

  // PasswordSyncTokenFetcher::Consumer
  void OnTokenCreated(const std::string& sync_token) override;
  void OnTokenFetched(const std::string& sync_token) override;
  void OnTokenVerified(bool is_valid) override;
  void OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType error_type) override;

  // Checks user's credentials.
  // In case of success, OnAuthSuccess will be triggered.
  // |user_context| has the user's credentials that needs to be checked.
  // |callback| is used in case the user's password does not match the
  // password that is used in encrypting his data by cryptohome
  void CheckCredentials(const UserContext& user_context,
                        PasswordChangedCallback callback);

  // Change the user's old password with the new one which is used to
  // verify the user on an IdP.
  void UpdateUserPassword(const std::string& old_password);

  // AuthStatusConsumer:
  // Shows password changed dialog.
  void OnAuthFailure(const chromeos::AuthFailure& error) override;

  // Unlocks the screen if active user successfully verified the password
  // with an IdP.
  void OnAuthSuccess(const UserContext& user_context) override;

  // Create and show lockscreen re-authentication dialog.
  void CreateAndShowDialog();

  // Dismiss lockscreen re-authentication dialog.
  void DismissDialog();

  // Reset lockscreen re-authentication dialog.
  void ResetDialog();

 private:
  void UpdateOnlineAuth();
  // Password sync token API calls.
  void CreateTokenAsync();
  void FetchTokenAsync();

  Profile* const primary_profile_;
  UserContext user_context_;
  const base::Clock* clock_;
  const user_manager::User* const primary_user_;
  ReauthenticationReason lock_screen_reauth_reason_ =
      ReauthenticationReason::kNone;
  proximity_auth::ScreenlockBridge* screenlock_bridge_;
  std::unique_ptr<PasswordSyncTokenFetcher> password_sync_token_fetcher_;

  // Used to authenticate the user.
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;
  scoped_refptr<CryptohomeAuthenticator> authenticator_;

  // Used to create dialog to authenticate the user on lockscreen.
  std::unique_ptr<LockScreenStartReauthDialog> lock_screen_start_reauth_dialog_;

  PasswordChangedCallback password_changed_callback_;

  friend class InSessionPasswordSyncManagerTest;
  friend class InSessionPasswordSyncManagerFactory;

  base::WeakPtrFactory<InSessionPasswordSyncManager> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_IN_SESSION_PASSWORD_SYNC_MANAGER_H_
