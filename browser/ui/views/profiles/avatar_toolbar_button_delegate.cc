// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"

#include "base/check_op.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/sync/driver/profile_sync_service.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {

constexpr base::TimeDelta kIdentityAnimationDuration =
    base::TimeDelta::FromSeconds(3);

constexpr base::TimeDelta kAvatarHighlightAnimationDuration =
    base::TimeDelta::FromSeconds(2);

ProfileAttributesStorage& GetProfileAttributesStorage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  return GetProfileAttributesStorage().GetProfileAttributesWithPath(
      profile->GetPath());
}

bool IsGenericProfile(const ProfileAttributesEntry& entry) {
  return entry.GetAvatarIconIndex() == 0 &&
         GetProfileAttributesStorage().GetNumberOfProfiles() == 1;
}

// Returns the avatar image for the current profile. May be called only in
// "normal" states where the user is guaranteed to have an avatar image (i.e.
// not kGuestSession and not kIncognitoProfile).
gfx::Image GetAvatarImage(Profile* profile,
                          const gfx::Image& user_identity_image,
                          int preferred_size) {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  if (!entry) {  // This can happen if the user deletes the current profile.
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  // TODO(crbug.com/1012179): it should suffice to call entry->GetAvatarIcon().
  // For this to work well, this class needs to observe ProfileAttributesStorage
  // instead of (or on top of) IdentityManager. Only then we can rely on |entry|
  // being up to date (as the storage also observes IdentityManager so there's
  // no guarantee on the order of notifications).
  if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture())
    return *entry->GetGAIAPicture();

  // Show |user_identity_image| when the following conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't syncing
  //  - the profile icon wasn't explicitly changed
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!user_identity_image.IsEmpty() &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      entry->IsUsingDefaultAvatar()) {
    return user_identity_image;
  }

  return entry->GetAvatarIcon(preferred_size);
}

// TODO(crbug.com/1125474): Replace IsGuest(profile) calls with
// Profile::IsGuestProfile() after IsEphemeralGuestProfile is fully migrated.
bool IsGuest(Profile* profile) {
  return profile->IsGuestSession() || profile->IsEphemeralGuestProfile();
}

}  // namespace

AvatarToolbarButtonDelegate::AvatarToolbarButtonDelegate(
    AvatarToolbarButton* button,
    Profile* profile)
    : avatar_toolbar_button_(button),
      profile_(profile),
      last_avatar_error_(sync_ui_util::GetAvatarSyncErrorType(profile)) {
  profile_observation_.Observe(&GetProfileAttributesStorage());

  if (auto* sync_service = ProfileSyncServiceFactory::GetForProfile(profile_))
    sync_service_observation_.Observe(sync_service);

  AvatarToolbarButton::State state = GetState();
  if (state == AvatarToolbarButton::State::kIncognitoProfile ||
      state == AvatarToolbarButton::State::kGuestSession) {
    BrowserList::AddObserver(this);
  } else {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    identity_manager_observation_.Observe(identity_manager);
    if (identity_manager->AreRefreshTokensLoaded())
      OnRefreshTokensLoaded();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(chromeos::features::kAvatarToolbarButton)) {
    // On CrOS this button should only show as badging for Incognito and Guest
    // sessions. It's only enabled for Incognito where a menu is available for
    // closing all Incognito windows.
    avatar_toolbar_button_->SetEnabled(
        state == AvatarToolbarButton::State::kIncognitoProfile);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

AvatarToolbarButtonDelegate::~AvatarToolbarButtonDelegate() {
  BrowserList::RemoveObserver(this);
}

std::u16string AvatarToolbarButtonDelegate::GetProfileName() const {
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  return profiles::GetAvatarNameForProfile(profile_->GetPath());
}

std::u16string AvatarToolbarButtonDelegate::GetShortProfileName() const {
  return signin_ui_util::GetShortProfileIdentityToDisplay(
      *GetProfileAttributesEntry(profile_), profile_);
}

gfx::Image AvatarToolbarButtonDelegate::GetGaiaAccountImage() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    base::Optional<AccountInfo> account_info =
        identity_manager
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
                identity_manager->GetPrimaryAccountId(
                    signin::ConsentLevel::kSignin));
    if (account_info.has_value())
      return account_info->account_image;
  }
  return gfx::Image();
}

gfx::Image AvatarToolbarButtonDelegate::GetProfileAvatarImage(
    gfx::Image gaia_account_image,
    int preferred_size) const {
  return GetAvatarImage(profile_, gaia_account_image, preferred_size);
}

int AvatarToolbarButtonDelegate::GetWindowCount() const {
  if (IsGuest(profile_))
    return BrowserList::GetGuestBrowserCount();
  DCHECK(profile_->IsOffTheRecord());
  return BrowserList::GetOffTheRecordBrowsersActiveForProfile(profile_);
}

AvatarToolbarButton::State AvatarToolbarButtonDelegate::GetState() const {
  if (IsGuest(profile_))
    return AvatarToolbarButton::State::kGuestSession;

  // Return |kIncognitoProfile| state for all OffTheRecord profile types except
  // guest mode.
  if (profile_->IsOffTheRecord())
    return AvatarToolbarButton::State::kIncognitoProfile;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry ||  // This can happen if the user deletes the current profile.
      (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
       IsGenericProfile(*entry))) {
    return AvatarToolbarButton::State::kGenericProfile;
  }

  if (identity_animation_state_ == IdentityAnimationState::kShowing) {
    return AvatarToolbarButton::State::kAnimatedUserIdentity;
  }

  if (!ProfileSyncServiceFactory::IsSyncAllowed(profile_) ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return AvatarToolbarButton::State::kNormal;
  }

  // Show any existing sync errors.
  const sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetAvatarSyncErrorType(profile_);
  if (error == sync_ui_util::AUTH_ERROR &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_)) {
    return AvatarToolbarButton::State::kSyncPaused;
  }

  if (error == sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR)
    return AvatarToolbarButton::State::kPasswordsOnlySyncError;

  return error == sync_ui_util::NO_SYNC_ERROR
             ? AvatarToolbarButton::State::kNormal
             : AvatarToolbarButton::State::kSyncError;
}

void AvatarToolbarButtonDelegate::ShowHighlightAnimation() {
  signin_ui_util::RecordAvatarIconHighlighted(profile_);
  highlight_animation_visible_ = true;
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kGuestSession);
  avatar_toolbar_button_->UpdateText();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButtonDelegate::HideHighlightAnimation,
                     weak_ptr_factory_.GetWeakPtr()),
      kAvatarHighlightAnimationDuration);
}

bool AvatarToolbarButtonDelegate::IsHighlightAnimationVisible() const {
  return highlight_animation_visible_;
}

void AvatarToolbarButtonDelegate::MaybeShowIdentityAnimation(
    const gfx::Image& gaia_account_image) {
  // TODO(crbug.com/990286): Get rid of this logic completely when we cache the
  // Google account image in the profile cache and thus it is always available.
  if (identity_animation_state_ != IdentityAnimationState::kWaitingForImage ||
      gaia_account_image.IsEmpty()) {
    return;
  }

  // Check that the user is still signed in. See https://crbug.com/1025674
  if (!IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin)) {
    identity_animation_state_ = IdentityAnimationState::kNotShowing;
    return;
  }

  ShowIdentityAnimation();
}

void AvatarToolbarButtonDelegate::SetHasInProductHelpPromo(bool has_promo) {
  if (has_in_product_help_promo_ == has_promo)
    return;

  has_in_product_help_promo_ = has_promo;
  // Trigger a new animation, even if the IPH is being removed. This keeps the
  // pill open a little more and avoids jankiness caused by the two animations
  // (IPH and identity pill) happening concurrently.
  // See https://crbug.com/1198907
  ShowIdentityAnimation();
}

void AvatarToolbarButtonDelegate::NotifyClick() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnMouseExited() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnBlur() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnHighlightChanged() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnBrowserAdded(Browser* browser) {
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnBrowserRemoved(Browser* browser) {
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnProfileAdded(
    const base::FilePath& profile_path) {
  // Adding any profile changes the profile count, we might go from showing a
  // generic avatar button to profile pictures here. Update icon accordingly.
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  // Removing a profile changes the profile count, we might go from showing
  // per-profile icons back to a generic avatar icon. Update icon accordingly.
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnRefreshTokensLoaded() {
  if (refresh_tokens_loaded_) {
    // This is possible, if |AvatarToolbarButtonDelegate::Init| is called within
    // the loop in |IdentityManager::OnRefreshTokensLoaded()| to notify
    // observers. In that case, |OnRefreshTokensLoaded| will be called twice,
    // once from |AvatarToolbarButtonDelegate::Init| and another time from the
    // |IdentityManager|. This happens for new signed in profiles.
    // See https://crbug.com/1035480
    return;
  }

  refresh_tokens_loaded_ = true;
  if (!signin_ui_util::ShouldShowAnimatedIdentityOnOpeningWindow(
          GetProfileAttributesStorage(), profile_)) {
    return;
  }
  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  if (account.IsEmpty())
    return;
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnStateChanged(syncer::SyncService*) {
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetAvatarSyncErrorType(profile_);
  if (last_avatar_error_ == error)
    return;

  last_avatar_error_ = error;
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnUserIdentityChanged() {
  signin_ui_util::RecordAnimatedIdentityTriggered(profile_);
  identity_animation_state_ = IdentityAnimationState::kWaitingForImage;
  // If we already have a gaia image, the pill will be immediately displayed by
  // UpdateIcon().
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout() {
  --identity_animation_timeout_count_;
  // If the count is > 0, there's at least one more pending
  // OnIdentityAnimationTimeout() that will hide it after the proper delay.
  if (identity_animation_timeout_count_ > 0)
    return;

  DCHECK_EQ(identity_animation_state_, IdentityAnimationState::kShowing);
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::MaybeHideIdentityAnimation() {
  // No-op if not showing or if the timeout hasn't passed, yet.
  if (identity_animation_state_ != IdentityAnimationState::kShowing ||
      identity_animation_timeout_count_ > 0) {
    return;
  }

  // Keep identity visible if this button is in use (hovered or has focus) or
  // has an associated In-Product-Help promo. We should not move things around
  // when the user wants to click on |this| or another button in the parent.
  if (avatar_toolbar_button_->IsMouseHovered() ||
      avatar_toolbar_button_->HasFocus() || has_in_product_help_promo_) {
    return;
  }

  identity_animation_state_ = IdentityAnimationState::kNotShowing;
  // Update the text to the pre-shown state. This also makes sure that we now
  // reflect changes that happened while the identity pill was shown.
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::HideHighlightAnimation() {
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kGuestSession);
  highlight_animation_visible_ = false;
  avatar_toolbar_button_->UpdateText();
  avatar_toolbar_button_->NotifyHighlightAnimationFinished();
}

void AvatarToolbarButtonDelegate::ShowIdentityAnimation() {
  identity_animation_state_ = IdentityAnimationState::kShowing;
  avatar_toolbar_button_->UpdateText();

  // Hide the pill after a while.
  ++identity_animation_timeout_count_;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kIdentityAnimationDuration);
}
