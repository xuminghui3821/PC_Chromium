// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/cookie_reminter_factory.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/cookie_reminter.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/signin/signin_bridge.h"
#include "ui/android/view_android.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/public/base/signin_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace signin {

const void* const kManageAccountsHeaderReceivedUserDataKey =
    &kManageAccountsHeaderReceivedUserDataKey;

const char kChromeMirrorHeaderSource[] = "Chrome";

namespace {

// Key for RequestDestructionObserverUserData.
const void* const kRequestDestructionObserverUserDataKey =
    &kRequestDestructionObserverUserDataKey;

// TODO(droger): Remove this delay when the Dice implementation is finished on
// the server side.
int g_dice_account_reconcilor_blocked_delay_ms = 1000;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";

// Refcounted wrapper that facilitates creating and deleting a
// AccountReconcilor::Lock.
class AccountReconcilorLockWrapper
    : public base::RefCountedThreadSafe<AccountReconcilorLockWrapper> {
 public:
  explicit AccountReconcilorLockWrapper(
      const content::WebContents::Getter& web_contents_getter) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = web_contents_getter.Run();
    if (!web_contents)
      return;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    AccountReconcilor* account_reconcilor =
        AccountReconcilorFactory::GetForProfile(profile);
    account_reconcilor_lock_ =
        std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
  }

  void DestroyAfterDelay() {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(base::DoNothing::Once<
                           scoped_refptr<AccountReconcilorLockWrapper>>(),
                       base::RetainedRef(this)),
        base::TimeDelta::FromMilliseconds(
            g_dice_account_reconcilor_blocked_delay_ms));
  }

 private:
  friend class base::RefCountedThreadSafe<AccountReconcilorLockWrapper>;
  ~AccountReconcilorLockWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  std::unique_ptr<AccountReconcilor::Lock> account_reconcilor_lock_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorLockWrapper);
};

// Returns true if the account reconcilor needs be be blocked while a Gaia
// sign-in request is in progress.
//
// The account reconcilor must be blocked on all request that may change the
// Gaia authentication cookies. This includes:
// * Main frame  requests.
// * XHR requests having Gaia URL as referrer.
bool ShouldBlockReconcilorForRequest(ChromeRequestAdapter* request) {
  if (request->GetRequestDestination() ==
      network::mojom::RequestDestination::kDocument) {
    return true;
  }

  return request->IsFetchLikeAPI() &&
         gaia::IsGaiaSignonRealm(request->GetReferrerOrigin());
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class RequestDestructionObserverUserData : public base::SupportsUserData::Data {
 public:
  explicit RequestDestructionObserverUserData(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  ~RequestDestructionObserverUserData() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestDestructionObserverUserData);
};

// This user data is used as a marker that a Mirror header was found on the
// redirect chain. It does not contain any data, its presence is enough to
// indicate that a header has already be found on the request.
class ManageAccountsHeaderReceivedUserData
    : public base::SupportsUserData::Data {};

// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeader(
    ManageAccountsParams manage_accounts_params,
    const content::WebContents::Getter& web_contents_getter) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(OS_ANDROID)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GAIAServiceType service_type = manage_accounts_params.service_type;
  DCHECK_NE(GAIA_SERVICE_TYPE_NONE, service_type);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile))
      << "Gaia should not send the X-Chrome-Manage-Accounts header "
      << "when Mirror is disabled.";
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnReceivedManageAccountsResponse(service_type);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) ||
        // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      account_reconcilor->GetState());

  bool should_ignore_guest_webview = true;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The mirror headers from some guest web views need to be processed.
  should_ignore_guest_webview =
      HeaderModificationDelegateImpl::ShouldIgnoreGuestWebViewRequest(
          web_contents);
#endif

  // Do not do anything if the navigation happened in the "background".
  if (!chrome::FindBrowserWithWebContents(web_contents) &&
      should_ignore_guest_webview) {
    return;
  }

  // Record the service type.
  base::UmaHistogramEnumeration("AccountManager.ManageAccountsServiceType",
                                service_type);

  // Ignore response to background request from another profile, so dialogs are
  // not displayed in the wrong profile when using multiprofile mode.
  if (profile != ProfileManager::GetActiveUserProfile())
    return;

  // The only allowed operations are:
  // 1. Going Incognito.
  // 2. Displaying a reauthentication window: Enterprise GSuite Accounts could
  //    have been forced through an online in-browser sign-in for sensitive
  //    webpages, thereby decreasing their session validity. After their session
  //    expires, they will receive a "Mirror" re-authentication request for all
  //    Google web properties. Another case when this can be triggered is
  //    https://crbug.com/1012649.
  // 3. Displaying the Account Manager for managing accounts.

  // 1. Going incognito.
  if (service_type == GAIA_SERVICE_TYPE_INCOGNITO) {
    chrome::NewIncognitoWindow(profile);
    return;
  }

  // 2. Displaying a reauthentication window
  if (!manage_accounts_params.email.empty()) {
    // TODO(https://crbug.com/1177728): enable this for lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Do not display the re-authentication dialog if this event was triggered
    // by supervision being enabled for an account.  In this situation, a
    // complete signout is required.
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    if (service && service->signout_required_after_supervision_enabled()) {
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    base::UmaHistogramBoolean("AccountManager.MirrorReauthenticationRequest",
                              true);

    // Child users shouldn't get the re-authentication dialog for primary
    // account. Log out all accounts to re-mint the cookies.
    // (See the reason below.)
    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (profile->IsChild() &&
        gaia::AreEmailsSame(primary_account.email,
                            manage_accounts_params.email)) {
      identity_manager->GetAccountsCookieMutator()->LogOutAllAccounts(
          gaia::GaiaSource::kChromeOS,
          base::DoNothing::Once<const GoogleServiceAuthError&>());
      return;
    }

    // The account's cookie is invalid but the cookie has not been removed by
    // |AccountReconcilor|. Ideally, this should not happen. At this point,
    // |AccountReconcilor| cannot detect this state because its source of truth
    // (/ListAccounts) is giving us false positives (claiming an invalid account
    // to be valid). We need to store that this account's cookie is actually
    // invalid, so that if/when this account is re-authenticated, we can force a
    // reconciliation for this account instead of treating it as a no-op.
    // See https://crbug.com/1012649 for details.
    base::Optional<AccountInfo> maybe_account_info =
        identity_manager
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                manage_accounts_params.email);
    if (maybe_account_info.has_value()) {
      CookieReminter* const cookie_reminter =
          CookieReminterFactory::GetForProfile(profile);
      cookie_reminter->ForceCookieRemintingOnNextTokenUpdate(
          maybe_account_info.value());
    }

    // Display a re-authentication dialog.
    ::GetAccountManagerFacade(profile->GetPath().value())
        ->ShowReauthAccountDialog(account_manager::AccountManagerFacade::
                                      AccountAdditionSource::kContentArea,
                                  manage_accounts_params.email);
    return;
  }

  // 3. Displaying the Account Manager for managing accounts.
  if (base::FeatureList::IsEnabled(switches::kUseAccountManagerFacade)) {
    ::GetAccountManagerFacade(profile->GetPath().value())
        ->ShowManageAccountsSettings();
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kMyAccountsSubpagePath);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return;

#elif defined(OS_ANDROID)
  if (manage_accounts_params.show_consistency_promo &&
      base::FeatureList::IsEnabled(kMobileIdentityConsistency)) {
    auto* window = web_contents->GetNativeView()->GetWindowAndroid();
    if (!window) {
      // The page is prefetched in the background, ignore the header.
      // See https://crbug.com/1145031#c5 for details.
      return;
    }
    SigninBridge::OpenAccountPickerBottomSheet(
        window, manage_accounts_params.continue_url.empty()
                    ? chrome::kChromeUINativeNewTabURL
                    : manage_accounts_params.continue_url);
    return;
  }
  if (service_type == signin::GAIA_SERVICE_TYPE_INCOGNITO) {
    GURL url(manage_accounts_params.continue_url.empty()
                 ? chrome::kChromeUINativeNewTabURL
                 : manage_accounts_params.continue_url);
    web_contents->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::OFF_THE_RECORD,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  } else {
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());
    auto* window = web_contents->GetNativeView()->GetWindowAndroid();
    if (!window)
      return;
    SigninBridge::OpenAccountManagementScreen(window, service_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Creates a DiceTurnOnSyncHelper.
void CreateDiceTurnOnSyncHelper(Profile* profile,
                                signin_metrics::AccessPoint access_point,
                                signin_metrics::PromoAction promo_action,
                                signin_metrics::Reason reason,
                                content::WebContents* web_contents,
                                const CoreAccountId& account_id) {
  DCHECK(profile);
  Browser* browser = web_contents
                         ? chrome::FindBrowserWithWebContents(web_contents)
                         : chrome::FindBrowserWithProfile(profile);
  // DiceTurnSyncOnHelper is suicidal (it will kill itself once it finishes
  // enabling sync).
  new DiceTurnSyncOnHelper(
      profile, browser, access_point, promo_action, reason, account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
}

// Shows UI for signin errors.
void ShowDiceSigninError(Profile* profile,
                         content::WebContents* web_contents,
                         const SigninUIError& error) {
  DCHECK(profile);
  Browser* browser = web_contents
                         ? chrome::FindBrowserWithWebContents(web_contents)
                         : chrome::FindBrowserWithProfile(profile);
  LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(browser,
                                                                    error);
}

void ProcessDiceHeader(
    const DiceResponseParams& dice_params,
    const content::WebContents::Getter& web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(!profile->IsOffTheRecord());

  // Ignore Dice response headers if Dice is not enabled.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile))
    return;

  DiceResponseHandler* dice_response_handler =
      DiceResponseHandler::GetForProfile(profile);
  dice_response_handler->ProcessDiceHeader(
      dice_params,
      std::make_unique<ProcessDiceHeaderDelegateImpl>(
          web_contents, base::BindOnce(&CreateDiceTurnOnSyncHelper),
          base::BindOnce(&ShowDiceSigninError)));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(ResponseAdapter* response,
                                         bool is_off_the_record) {
  CHECK(gaia::IsGaiaSignonRealm(response->GetOrigin()));

  if (!response->IsMainFrame())
    return;

  const net::HttpResponseHeaders* response_headers = response->GetHeaders();
  if (!response_headers)
    return;

  std::string header_value;
  if (!response_headers->GetNormalizedHeader(kChromeManageAccountsHeader,
                                             &header_value)) {
    return;
  }

  if (is_off_the_record) {
    NOTREACHED() << "Gaia should not send the X-Chrome-Manage-Accounts header "
                 << "in incognito.";
    return;
  }

  ManageAccountsParams params = BuildManageAccountsParams(header_value);
  // If the request does not have a response header or if the header contains
  // garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  // Only process one mirror header per request (multiple headers on the same
  // redirect chain are ignored).
  if (response->GetUserData(kManageAccountsHeaderReceivedUserDataKey)) {
    LOG(ERROR) << "Multiple X-Chrome-Manage-Accounts headers on a redirect "
               << "chain, ignoring";
    return;
  }

  response->SetUserData(
      kManageAccountsHeaderReceivedUserDataKey,
      std::make_unique<ManageAccountsHeaderReceivedUserData>());

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(ProcessMirrorHeader, params,
                                response->GetWebContentsGetter()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProcessDiceResponseHeaderIfExists(ResponseAdapter* response,
                                       bool is_off_the_record) {
  CHECK(gaia::IsGaiaSignonRealm(response->GetOrigin()));

  if (is_off_the_record)
    return;

  const net::HttpResponseHeaders* response_headers = response->GetHeaders();
  if (!response_headers)
    return;

  std::string header_value;
  DiceResponseParams params;
  if (response_headers->GetNormalizedHeader(kDiceResponseHeader,
                                            &header_value)) {
    params = BuildDiceSigninResponseParams(header_value);
    // The header must be removed for privacy reasons, so that renderers never
    // have access to the authorization code.
    response->RemoveHeader(kDiceResponseHeader);
  } else if (response_headers->GetNormalizedHeader(kGoogleSignoutResponseHeader,
                                                   &header_value)) {
    params = BuildDiceSignoutResponseParams(header_value);
  }

  // If the request does not have a response header or if the header contains
  // garbage, then |user_intention| is set to |NONE|.
  if (params.user_intention == DiceAction::NONE)
    return;

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(ProcessDiceHeader, std::move(params),
                                response->GetWebContentsGetter()));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

ChromeRequestAdapter::ChromeRequestAdapter(
    const GURL& url,
    const net::HttpRequestHeaders& original_headers,
    net::HttpRequestHeaders* modified_headers,
    std::vector<std::string>* headers_to_remove)
    : RequestAdapter(url,
                     original_headers,
                     modified_headers,
                     headers_to_remove) {}

ChromeRequestAdapter::~ChromeRequestAdapter() = default;

ResponseAdapter::ResponseAdapter() = default;

ResponseAdapter::~ResponseAdapter() = default;

void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms) {
  g_dice_account_reconcilor_blocked_delay_ms = delay_ms;
}

void FixAccountConsistencyRequestHeader(
    ChromeRequestAdapter* request,
    const GURL& redirect_url,
    bool is_off_the_record,
    int incognito_availibility,
    AccountConsistencyMethod account_consistency,
    std::string gaia_id,
    const base::Optional<bool>& is_child_account,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    bool is_secondary_account_addition_allowed,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool is_sync_enabled,
    std::string signin_scoped_device_id,
#endif
    content_settings::CookieSettings* cookie_settings) {
  if (is_off_the_record)
    return;  // Account consistency is disabled in incognito.

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (incognito_availibility == IncognitoModePrefs::DISABLED ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabled()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!is_secondary_account_addition_allowed) {
    account_consistency = AccountConsistencyMethod::kMirror;
    // Can't add new accounts.
    profile_mode_mask |= PROFILE_MODE_ADD_ACCOUNT_DISABLED;
  }
#endif

  // If new url is eligible to have the header, add it, otherwise remove it.

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Dice header:
  bool dice_header_added = AppendOrRemoveDiceRequestHeader(
      request, redirect_url, gaia_id, is_sync_enabled, account_consistency,
      cookie_settings, signin_scoped_device_id);

  // Block the AccountReconcilor while the Dice requests are in flight. This
  // allows the DiceReponseHandler to process the response before the reconcilor
  // starts.
  if (dice_header_added && ShouldBlockReconcilorForRequest(request)) {
    auto lock_wrapper = base::MakeRefCounted<AccountReconcilorLockWrapper>(
        request->GetWebContentsGetter());
    // On destruction of the request |lock_wrapper| will be released.
    request->SetDestructionCallback(base::BindOnce(
        &AccountReconcilorLockWrapper::DestroyAfterDelay, lock_wrapper));
  }
#endif

  // Mirror header:
  AppendOrRemoveMirrorRequestHeader(
      request, redirect_url, gaia_id, is_child_account, account_consistency,
      cookie_settings, profile_mode_mask, kChromeMirrorHeaderSource,
      /*force_account_consistency=*/false);
}

void ProcessAccountConsistencyResponseHeaders(ResponseAdapter* response,
                                              const GURL& redirect_url,
                                              bool is_off_the_record) {
  if (!gaia::IsGaiaSignonRealm(response->GetOrigin()))
    return;

  // See if the response contains the X-Chrome-Manage-Accounts header. If so
  // show the profile avatar bubble so that user can complete signin/out
  // action the native UI.
  ProcessMirrorResponseHeaderIfExists(response, is_off_the_record);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Process the Dice header: on sign-in, exchange the authorization code for a
  // refresh token, on sign-out just follow the sign-out URL.
  ProcessDiceResponseHeaderIfExists(response, is_off_the_record);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace signin
