// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"

#include <cmath>
#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_migration_constants.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_feedback.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Path to the mount point to check the available space.
constexpr char kCheckStoragePath[] = "/home";

constexpr char kUserActionStartMigration[] = "startMigration";
constexpr char kUserActionSkipMigration[] = "skipMigration";
constexpr char kUserActionRequestRestartOnLowStorage[] =
    "requestRestartOnLowStorage";
constexpr char kUserActionRequestRestartOnFailure[] = "requestRestartOnFailure";
constexpr char kUserActionOpenFeedbackDialog[] = "openFeedbackDialog";

// UMA names.
constexpr char kUmaNameFirstScreen[] = "Cryptohome.MigrationUI.FirstScreen";
constexpr char kUmaNameUserChoice[] = "Cryptohome.MigrationUI.UserChoice";
constexpr char kUmaNameMigrationResult[] =
    "Cryptohome.MigrationUI.MigrationResult";
constexpr char kUmaNameRemoveCryptohomeResult[] =
    "Cryptohome.MigrationUI.RemoveCryptohomeResult";
constexpr char kUmaNameConsumedBatteryPercent[] =
    "Cryptohome.MigrationUI.ConsumedBatteryPercent";
constexpr char kUmaNameVisibleScreen[] = "Cryptohome.MigrationUI.VisibleScreen";

// This enum must match the numbering for MigrationUIFirstScreen in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before FIRST_SCREEN_COUNT.
enum class FirstScreen {
  FIRST_SCREEN_READY = 0,
  FIRST_SCREEN_RESUME = 1,
  FIRST_SCREEN_LOW_STORAGE = 2,
  FIRST_SCREEN_ARC_KIOSK = 3,
  FIRST_SCREEN_START_AUTOMATICALLY = 4,
  FIRST_SCREEN_RESUME_MINIMAL = 5,
  FIRST_SCREEN_START_AUTOMATICALLY_MINIMAL = 6,
  FIRST_SCREEN_COUNT
};

// This enum must match the numbering for MigrationUIUserChoice in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before USER_CHOICE_COUNT.
enum class UserChoice {
  USER_CHOICE_UPDATE = 0,
  USER_CHOICE_SKIP = 1,
  USER_CHOICE_RESTART_ON_FAILURE = 2,
  USER_CHOICE_RESTART_ON_LOW_STORAGE = 3,
  USER_CHOICE_REPORT_AN_ISSUE = 4,
  USER_CHOICE_COUNT
};

// This enum must match the numbering for MigrationUIMigrationResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class MigrationResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  GENERAL_FAILURE_IN_NEW_MIGRATION = 2,
  GENERAL_FAILURE_IN_RESUMED_MIGRATION = 3,
  REQUEST_FAILURE_IN_NEW_MIGRATION = 4,
  REQUEST_FAILURE_IN_RESUMED_MIGRATION = 5,
  MOUNT_FAILURE_IN_NEW_MIGRATION = 6,
  MOUNT_FAILURE_IN_RESUMED_MIGRATION = 7,
  SUCCESS_IN_ARC_KIOSK_MIGRATION = 8,
  GENERAL_FAILURE_IN_ARC_KIOSK_MIGRATION = 9,
  REQUEST_FAILURE_IN_ARC_KIOSK_MIGRATION = 10,
  MOUNT_FAILURE_IN_ARC_KIOSK_MIGRATION = 11,
  COUNT
};

// This enum must match the numbering for MigrationUIRemoveCryptohomeResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class RemoveCryptohomeResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  FAILURE_IN_NEW_MIGRATION = 2,
  FAILURE_IN_RESUMED_MIGRATION = 3,
  SUCCESS_IN_ARC_KIOSK_MIGRATION = 4,
  FAILURE_IN_ARC_KIOSK_MIGRATION = 5,
  COUNT
};

bool IsTestingUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

// Wrapper functions for histogram macros to avoid duplication of expanded code.
void RecordFirstScreen(FirstScreen first_screen) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameFirstScreen, first_screen,
                            FirstScreen::FIRST_SCREEN_COUNT);
}

void RecordUserChoice(UserChoice user_choice) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameUserChoice, user_choice,
                            UserChoice::USER_CHOICE_COUNT);
}

void RecordMigrationResult(MigrationResult migration_result) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameMigrationResult, migration_result,
                            MigrationResult::COUNT);
}

void RecordMigrationResultSuccess(bool resume, bool arc_kiosk) {
  if (arc_kiosk)
    RecordMigrationResult(MigrationResult::SUCCESS_IN_ARC_KIOSK_MIGRATION);
  else if (resume)
    RecordMigrationResult(MigrationResult::SUCCESS_IN_RESUMED_MIGRATION);
  else
    RecordMigrationResult(MigrationResult::SUCCESS_IN_NEW_MIGRATION);
}

void RecordMigrationResultGeneralFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::GENERAL_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(
        MigrationResult::GENERAL_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::GENERAL_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordMigrationResultRequestFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::REQUEST_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(
        MigrationResult::REQUEST_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::REQUEST_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordMigrationResultMountFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::MOUNT_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(MigrationResult::MOUNT_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::MOUNT_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordRemoveCryptohomeResult(RemoveCryptohomeResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameRemoveCryptohomeResult, result,
                            RemoveCryptohomeResult::COUNT);
}

void RecordRemoveCryptohomeResultSuccess(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_RESUMED_MIGRATION);
  } else {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_NEW_MIGRATION);
  }
}

void RecordRemoveCryptohomeResultFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_NEW_MIGRATION);
  }
}

// Chooses the value for the MigrationUIFirstScreen UMA stat. Not used for ARC
// kiosk.
FirstScreen GetFirstScreenForMode(chromeos::EncryptionMigrationMode mode) {
  switch (mode) {
    case chromeos::EncryptionMigrationMode::ASK_USER:
      return FirstScreen::FIRST_SCREEN_READY;
    case chromeos::EncryptionMigrationMode::START_MIGRATION:
      return FirstScreen::FIRST_SCREEN_START_AUTOMATICALLY;
    case chromeos::EncryptionMigrationMode::RESUME_MIGRATION:
      return FirstScreen::FIRST_SCREEN_RESUME;
  }
}

}  // namespace

namespace chromeos {

EncryptionMigrationScreen::EncryptionMigrationScreen(
    EncryptionMigrationScreenView* view)
    : BaseScreen(EncryptionMigrationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view) {
  free_disk_space_fetcher_ = base::BindRepeating(
      &base::SysInfo::AmountOfFreeDiskSpace, base::FilePath(kCheckStoragePath));
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

EncryptionMigrationScreen::~EncryptionMigrationScreen() {
  userdataauth_observer_.reset();
  power_manager_observer_.reset();
  if (view_)
    view_->SetDelegate(nullptr);
}

void EncryptionMigrationScreen::OnViewDestroyed(
    EncryptionMigrationScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void EncryptionMigrationScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void EncryptionMigrationScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void EncryptionMigrationScreen::SetUserContext(
    const UserContext& user_context) {
  DCHECK(view_);
  user_context_ = user_context;
}

void EncryptionMigrationScreen::SetMode(EncryptionMigrationMode mode) {
  DCHECK(view_);
  mode_ = mode;
  view_->SetIsResuming(IsStartImmediately());
}

void EncryptionMigrationScreen::SetSkipMigrationCallback(
    SkipMigrationCallback skip_migration_callback) {
  DCHECK(view_);
  skip_migration_callback_ = std::move(skip_migration_callback);
}

void EncryptionMigrationScreen::SetupInitialView() {
  DCHECK(view_);
  // Pass constant value(s) to the UI.
  view_->SetNecessaryBatteryPercent(arc::kMigrationMinimumBatteryPercent);

  // If old encryption is detected in ARC kiosk mode, skip all checks (user
  // confirmation, battery level, and remaining space) and start migration
  // immediately.
  if (IsArcKiosk()) {
    RecordFirstScreen(FirstScreen::FIRST_SCREEN_ARC_KIOSK);
    StartMigration();
    return;
  }
  power_manager_observer_ = std::make_unique<
      ScopedObserver<PowerManagerClient, PowerManagerClient::Observer>>(this);
  power_manager_observer_->Add(PowerManagerClient::Get());
  CheckAvailableStorage();
}

void EncryptionMigrationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionStartMigration) {
    HandleStartMigration();
  } else if (action_id == kUserActionSkipMigration) {
    HandleSkipMigration();
  } else if (action_id == kUserActionRequestRestartOnLowStorage) {
    HandleRequestRestartOnLowStorage();
  } else if (action_id == kUserActionRequestRestartOnFailure) {
    HandleRequestRestartOnFailure();
  } else if (action_id == kUserActionOpenFeedbackDialog) {
    HandleOpenFeedbackDialog();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void EncryptionMigrationScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (proto.has_battery_percent()) {
    if (!current_battery_percent_) {
      // If initial battery level is below the minimum, migration should start
      // automatically once the device is charged enough.
      if (proto.battery_percent() < arc::kMigrationMinimumBatteryPercent) {
        should_migrate_on_enough_battery_ = true;
        // If migration was forced by policy, stop forcing it (we don't want the
        // user to have to wait until the battery is charged).
        MaybeStopForcingMigration();
      }
    }
    current_battery_percent_ = proto.battery_percent();
  } else {
    // If battery level is not provided, we regard it as 100% to start migration
    // immediately.
    current_battery_percent_ = 100.0;
  }

  view_->SetBatteryState(
      *current_battery_percent_,
      *current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent,
      proto.battery_state() ==
          power_manager::PowerSupplyProperties_BatteryState_CHARGING);

  // If the migration was already requested and the battery level is enough now,
  // The migration should start immediately.
  if (*current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent &&
      should_migrate_on_enough_battery_) {
    should_migrate_on_enough_battery_ = false;
    StartMigration();
  }
}

void EncryptionMigrationScreen::HandleStartMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_UPDATE);
  WaitBatteryAndMigrate();
}

void EncryptionMigrationScreen::HandleSkipMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_SKIP);
  // If the user skips migration, we mount the cryptohome without performing the
  // migration by reusing UserContext and LoginPerformer which were used in the
  // previous attempt and dropping |is_forcing_dircrypto| flag in UserContext.
  // In this case, the user can not launch ARC apps in the session, and will be
  // asked to do the migration again in the next log-in attempt.
  if (!skip_migration_callback_.is_null()) {
    user_context_.SetIsForcingDircrypto(false);
    std::move(skip_migration_callback_).Run(user_context_);
  }
}

void EncryptionMigrationScreen::HandleRequestRestartOnLowStorage() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_LOW_STORAGE);
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login encryption migration low storage");
}

void EncryptionMigrationScreen::HandleRequestRestartOnFailure() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_FAILURE);
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login encryption migration failure");
}

void EncryptionMigrationScreen::HandleOpenFeedbackDialog() {
  RecordUserChoice(UserChoice::USER_CHOICE_REPORT_AN_ISSUE);
  const std::string description = base::StringPrintf(
      "Auto generated feedback for http://crbug.com/719266.\n"
      "(uniquifier:%s)",
      base::NumberToString(base::Time::Now().ToInternalValue()).c_str());
  login_feedback_ = std::make_unique<LoginFeedback>(Profile::FromWebUI(
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()));
  login_feedback_->Request(description, base::OnceClosure());
}

void EncryptionMigrationScreen::UpdateUIState(
    EncryptionMigrationScreenView::UIState state) {
  if (state == current_ui_state_)
    return;

  current_ui_state_ = state;
  view_->SetUIState(state);

  // When this handler is about to show the READY screen, we should get the
  // latest battery status and show it on the screen.
  if (state == EncryptionMigrationScreenView::READY)
    PowerManagerClient::Get()->RequestStatusUpdate();

  // We should request wake lock and not shut down on lid close during
  // migration.
  if (state == EncryptionMigrationScreenView::MIGRATING) {
    GetWakeLock()->RequestWakeLock();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(true);
  } else {
    GetWakeLock()->CancelWakeLock();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(false);
  }

  // Record which screen is visible to the user.
  // We record it after delay to make sure that the user was actually able
  // to see the screen (i.e. the screen is not just a flash).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EncryptionMigrationScreen::OnDelayedRecordVisibleScreen,
                     weak_ptr_factory_.GetWeakPtr(), state),
      base::TimeDelta::FromSeconds(1));
}

void EncryptionMigrationScreen::CheckAvailableStorage() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(free_disk_space_fetcher_),
      base::BindOnce(&EncryptionMigrationScreen::OnGetAvailableStorage,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreen::OnGetAvailableStorage(int64_t size) {
  if (size >= arc::kMigrationMinimumAvailableStorage || IsTestingUI()) {
    RecordFirstScreen(GetFirstScreenForMode(mode_));
    if (IsStartImmediately()) {
      WaitBatteryAndMigrate();
    } else {
      UpdateUIState(EncryptionMigrationScreenView::READY);
    }
  } else {
    RecordFirstScreen(FirstScreen::FIRST_SCREEN_LOW_STORAGE);
    view_->SetSpaceInfoInString(
        size /* availableSpaceSize */,
        arc::kMigrationMinimumAvailableStorage /* necessarySpaceSize */);
    UpdateUIState(EncryptionMigrationScreenView::NOT_ENOUGH_STORAGE);
  }
}

void EncryptionMigrationScreen::WaitBatteryAndMigrate() {
  if (current_battery_percent_) {
    if (*current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent) {
      StartMigration();
      return;
    } else {
      // If migration was forced by policy, stop forcing it (we don't want the
      // user to have to wait until the battery is charged).
      MaybeStopForcingMigration();
    }
  }
  UpdateUIState(EncryptionMigrationScreenView::READY);

  should_migrate_on_enough_battery_ = true;
  PowerManagerClient::Get()->RequestStatusUpdate();
}

void EncryptionMigrationScreen::StartMigration() {
  UpdateUIState(EncryptionMigrationScreenView::MIGRATING);
  if (current_battery_percent_)
    initial_battery_percent_ = *current_battery_percent_;

  // Mount the existing eCryptfs vault to a temporary location for migration.
  user_data_auth::MountRequest mount;
  *mount.mutable_account() = cryptohome::CreateAccountIdentifierFromAccountId(
      user_context_.GetAccountId());
  cryptohome::AuthorizationRequest auth_request;
  mount.set_to_migrate_from_ecryptfs(true);
  if (IsArcKiosk()) {
    mount.set_public_mount(true);
  } else {
    auth_request = CreateAuthorizationRequest();
  }
  *mount.mutable_authorization() = auth_request;
  UserDataAuthClient::Get()->Mount(
      mount, base::BindOnce(&EncryptionMigrationScreen::OnMountExistingVault,
                            weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreen::OnMountExistingVault(
    base::Optional<user_data_auth::MountReply> reply) {
  cryptohome::MountError return_code = user_data_auth::ReplyToMountError(reply);
  if (return_code != cryptohome::MOUNT_ERROR_NONE) {
    RecordMigrationResultMountFailure(IsResumingIncompleteMigration(),
                                      IsArcKiosk());
    UpdateUIState(EncryptionMigrationScreenView::MIGRATION_FAILED);
    LOG(ERROR) << "Mount existing vault failed. Error: " << return_code;
    return;
  }

  user_data_auth::StartMigrateToDircryptoRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId());
  userdataauth_observer_ = std::make_unique<base::ScopedObservation<
      UserDataAuthClient, UserDataAuthClient::Observer>>(this);
  userdataauth_observer_->Observe(UserDataAuthClient::Get());
  UserDataAuthClient::Get()->StartMigrateToDircrypto(
      request, base::BindOnce(&EncryptionMigrationScreen::OnMigrationRequested,
                              weak_ptr_factory_.GetWeakPtr()));
}

device::mojom::WakeLock* EncryptionMigrationScreen::GetWakeLock() {
  // |wake_lock_| is lazy bound and reused, even after a connection error.
  if (wake_lock_)
    return wake_lock_.get();

  mojo::PendingReceiver<device::mojom::WakeLock> receiver =
      wake_lock_.BindNewPipeAndPassReceiver();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther,
      "Encryption migration is in progress...", std::move(receiver));
  return wake_lock_.get();
}

void EncryptionMigrationScreen::RemoveCryptohome() {
  // Set invalid token status so that user is forced to go through Gaia on the
  // next sign-in.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      user_context_.GetAccountId(),
      user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);

  const cryptohome::Identification cryptohome_id(user_context_.GetAccountId());

  user_data_auth::RemoveRequest request;
  request.mutable_identifier()->set_account_id(cryptohome_id.id());
  UserDataAuthClient::Get()->Remove(
      request, base::BindOnce(&EncryptionMigrationScreen::OnRemoveCryptohome,
                              weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreen::OnRemoveCryptohome(
    base::Optional<user_data_auth::RemoveReply> reply) {
  cryptohome::MountError error = user_data_auth::ReplyToMountError(reply);
  if (error == cryptohome::MOUNT_ERROR_NONE) {
    RecordRemoveCryptohomeResultSuccess(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
  } else {
    LOG(ERROR) << "Removing cryptohome failed. return code: " << reply->error();
    RecordRemoveCryptohomeResultFailure(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
  }

  UpdateUIState(EncryptionMigrationScreenView::MIGRATION_FAILED);
}

cryptohome::AuthorizationRequest
EncryptionMigrationScreen::CreateAuthorizationRequest() {
  // |key| is created in the same manner as CryptohomeAuthenticator.
  const Key* key = user_context_.GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  cryptohome::AuthorizationRequest auth;
  cryptohome::Key* auth_key = auth.mutable_key();
  // Don't set the authorization's key label, implicitly setting it to an
  // empty string, which is a wildcard allowing any key to match. This is
  // necessary because cryptohomes created by Chrome OS M38 and older will have
  // a legacy key with no label while those created by Chrome OS M39 and newer
  // will have a key with the label kCryptohomeGAIAKeyLabel.
  auth_key->set_secret(key->GetSecret());
  return auth;
}

bool EncryptionMigrationScreen::IsArcKiosk() const {
  return user_context_.GetUserType() == user_manager::USER_TYPE_ARC_KIOSK_APP;
}

void EncryptionMigrationScreen::DircryptoMigrationProgress(
    const ::user_data_auth::DircryptoMigrationProgress& progress) {
  switch (progress.status()) {
    case user_data_auth::DircryptoMigrationStatus::
        DIRCRYPTO_MIGRATION_INITIALIZING:
      UpdateUIState(EncryptionMigrationScreenView::MIGRATING);
      break;
    case user_data_auth::DircryptoMigrationStatus::
        DIRCRYPTO_MIGRATION_IN_PROGRESS:
      UpdateUIState(EncryptionMigrationScreenView::MIGRATING);
      view_->SetMigrationProgress(
          static_cast<double>(progress.current_bytes()) /
          progress.total_bytes());
      break;
    case user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS:
      RecordMigrationResultSuccess(IsResumingIncompleteMigration(),
                                   IsArcKiosk());
      // Stop listening to the progress updates.
      userdataauth_observer_.reset();
      // If the battery level decreased during migration, record the consumed
      // battery level.
      if (current_battery_percent_ &&
          *current_battery_percent_ < initial_battery_percent_) {
        UMA_HISTOGRAM_PERCENTAGE(
            kUmaNameConsumedBatteryPercent,
            static_cast<int>(std::round(initial_battery_percent_ -
                                        *current_battery_percent_)));
      }
      // Restart immediately after successful migration.
      PowerManagerClient::Get()->RequestRestart(
          power_manager::REQUEST_RESTART_OTHER,
          "login encryption migration success");
      break;
    case user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_FAILED:
      RecordMigrationResultGeneralFailure(IsResumingIncompleteMigration(),
                                          IsArcKiosk());
      // Stop listening to the progress updates.
      userdataauth_observer_.reset();
      // Shows error screen after removing user directory is completed.
      RemoveCryptohome();
      break;
    default:
      break;
  }
}

void EncryptionMigrationScreen::OnMigrationRequested(
    base::Optional<user_data_auth::StartMigrateToDircryptoReply> reply) {
  if (!reply.has_value() ||
      reply->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "Requesting MigrateToDircrypto failed.";
    RecordMigrationResultRequestFailure(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
    UpdateUIState(EncryptionMigrationScreenView::MIGRATION_FAILED);
  }
}

void EncryptionMigrationScreen::OnDelayedRecordVisibleScreen(
    EncryptionMigrationScreenView::UIState ui_state) {
  if (current_ui_state_ != ui_state)
    return;

  // If |current_ui_state_| is not changed for a second, record the current
  // screen as a "visible" screen.
  UMA_HISTOGRAM_ENUMERATION(kUmaNameVisibleScreen, ui_state,
                            EncryptionMigrationScreenView::COUNT);
}

bool EncryptionMigrationScreen::IsResumingIncompleteMigration() const {
  return mode_ == EncryptionMigrationMode::RESUME_MIGRATION;
}

bool EncryptionMigrationScreen::IsStartImmediately() const {
  return mode_ == EncryptionMigrationMode::START_MIGRATION ||
         mode_ == EncryptionMigrationMode::RESUME_MIGRATION;
}

void EncryptionMigrationScreen::MaybeStopForcingMigration() {
  // |mode_| will be START_MIGRATION if migration was forced.
  // If an incomplete migration is being resumed, it would be RESUME_MIGRATION.
  // We only want to disable auto-starting migration in the first case.
  if (mode_ == EncryptionMigrationMode::START_MIGRATION)
    view_->SetIsResuming(false);
}

}  // namespace chromeos
