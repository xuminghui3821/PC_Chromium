// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/fake_android_sms_app_manager.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace settings {

namespace {

class TestMultideviceHandler : public MultideviceHandler {
 public:
  TestMultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::NotificationAccessManager* notification_access_manager,
      multidevice_setup::AndroidSmsPairingStateTracker*
          android_sms_pairing_state_tracker,
      android_sms::AndroidSmsAppManager* android_sms_app_manager)
      : MultideviceHandler(prefs,
                           multidevice_setup_client,
                           notification_access_manager,
                           android_sms_pairing_state_tracker,
                           android_sms_app_manager) {}
  ~TestMultideviceHandler() override = default;

  // Make public for testing.
  using MultideviceHandler::AllowJavascript;
  using MultideviceHandler::RegisterMessages;
  using MultideviceHandler::set_web_ui;
};

multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
GenerateDefaultFeatureStatesMap() {
  return multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap{
      {multidevice_setup::mojom::Feature::kBetterTogetherSuite,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kInstantTethering,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kMessages,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kSmartLock,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHub,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHubNotifications,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kWifiSync,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost},
      {multidevice_setup::mojom::Feature::kEche,
       multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost}};
}

void VerifyPageContentDict(
    const base::Value* value,
    multidevice_setup::mojom::HostStatus expected_host_status,
    const base::Optional<multidevice::RemoteDeviceRef>& expected_host_device,
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  const base::DictionaryValue* page_content_dict;
  EXPECT_TRUE(value->GetAsDictionary(&page_content_dict));

  int mode;
  EXPECT_TRUE(page_content_dict->GetInteger("mode", &mode));
  EXPECT_EQ(static_cast<int>(expected_host_status), mode);

  int better_together_state;
  EXPECT_TRUE(page_content_dict->GetInteger("betterTogetherState",
                                            &better_together_state));
  auto it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite);
  EXPECT_EQ(static_cast<int>(it->second), better_together_state);

  int instant_tethering_state;
  EXPECT_TRUE(page_content_dict->GetInteger("instantTetheringState",
                                            &instant_tethering_state));
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kInstantTethering);
  EXPECT_EQ(static_cast<int>(it->second), instant_tethering_state);

  int messages_state;
  EXPECT_TRUE(page_content_dict->GetInteger("messagesState", &messages_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kMessages);
  EXPECT_EQ(static_cast<int>(it->second), messages_state);

  int smart_lock_state;
  EXPECT_TRUE(
      page_content_dict->GetInteger("smartLockState", &smart_lock_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kSmartLock);
  EXPECT_EQ(static_cast<int>(it->second), smart_lock_state);

  int phone_hub_state;
  EXPECT_TRUE(page_content_dict->GetInteger("phoneHubState", &phone_hub_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kPhoneHub);
  EXPECT_EQ(static_cast<int>(it->second), phone_hub_state);

  int phone_hub_notifications_state;
  EXPECT_TRUE(page_content_dict->GetInteger("phoneHubNotificationsState",
                                            &phone_hub_notifications_state));
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubNotifications);
  EXPECT_EQ(static_cast<int>(it->second), phone_hub_notifications_state);

  int phone_hub_task_continuation_state;
  EXPECT_TRUE(page_content_dict->GetInteger(
      "phoneHubTaskContinuationState", &phone_hub_task_continuation_state));
  it = feature_states_map.find(
      multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation);
  EXPECT_EQ(static_cast<int>(it->second), phone_hub_task_continuation_state);

  int wifi_sync_state;
  EXPECT_TRUE(page_content_dict->GetInteger("wifiSyncState", &wifi_sync_state));
  it = feature_states_map.find(multidevice_setup::mojom::Feature::kWifiSync);
  EXPECT_EQ(static_cast<int>(it->second), wifi_sync_state);

  std::string host_device_name;
  if (expected_host_device) {
    EXPECT_TRUE(
        page_content_dict->GetString("hostDeviceName", &host_device_name));
    EXPECT_EQ(expected_host_device->name(), host_device_name);
  } else {
    EXPECT_FALSE(
        page_content_dict->GetString("hostDeviceName", &host_device_name));
  }
}

}  // namespace

class MultideviceHandlerTest : public testing::Test {
 protected:
  MultideviceHandlerTest()
      : test_device_(multidevice::CreateRemoteDeviceRefForTest()) {}
  ~MultideviceHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_web_ui_ = std::make_unique<content::TestWebUI>();

    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_notification_access_manager_ =
        std::make_unique<phonehub::FakeNotificationAccessManager>(
            phonehub::NotificationAccessManager::AccessStatus::
                kAvailableButNotGranted);
    fake_android_sms_pairing_state_tracker_ = std::make_unique<
        multidevice_setup::FakeAndroidSmsPairingStateTracker>();
    fake_android_sms_app_manager_ =
        std::make_unique<android_sms::FakeAndroidSmsAppManager>();

    prefs_ = std::make_unique<TestingPrefServiceSimple>();

    handler_ = std::make_unique<TestMultideviceHandler>(
        prefs_.get(), fake_multidevice_setup_client_.get(),
        fake_notification_access_manager_.get(),
        fake_android_sms_pairing_state_tracker_.get(),
        fake_android_sms_app_manager_.get());
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascript();

    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPhoneHub, chromeos::features::kEcheSWA}, {});
  }

  void CallGetPageContentData() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getPageContentData", &args);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    VerifyPageContent(call_data.arg3());
  }

  void CallRemoveHostDevice() {
    size_t num_remote_host_device_calls_before_call =
        fake_multidevice_setup_client()->num_remove_host_device_called();
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("removeHostDevice", &empty_args);
    EXPECT_EQ(num_remote_host_device_calls_before_call + 1u,
              fake_multidevice_setup_client()->num_remove_host_device_called());
  }

  void CallGetAndroidSmsInfo(bool expected_enabled, const GURL& expected_url) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getAndroidSmsInfo", &args);

    ASSERT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    ASSERT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(
        ContentSettingsPattern::FromURLNoWildcard(expected_url).ToString(),
        call_data.arg3()->FindKey("origin")->GetString());
    EXPECT_EQ(expected_enabled,
              call_data.arg3()->FindKey("enabled")->GetBool());
  }

  void CallAttemptNotificationSetup(bool has_access_been_granted) {
    fake_notification_access_manager()->SetAccessStatusInternal(
        has_access_been_granted
            ? phonehub::NotificationAccessManager::AccessStatus::kAccessGranted
            : phonehub::NotificationAccessManager::AccessStatus::
                  kAvailableButNotGranted);
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("attemptNotificationSetup",
                                         &empty_args);
  }

  void CallCancelNotificationSetup() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("cancelNotificationSetup",
                                         &empty_args);
  }

  void SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        std::make_pair(host_status, host_device));
    EXPECT_EQ(call_data_count_before_call + 2u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulateFeatureStatesUpdate(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_multidevice_setup_client_->SetFeatureStates(feature_states_map);
    EXPECT_EQ(call_data_count_before_call + 2u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void SimulatePairingStateUpdate(bool is_android_sms_pairing_complete) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_android_sms_pairing_state_tracker_->SetPairingComplete(
        is_android_sms_pairing_complete);
    EXPECT_EQ(call_data_count_before_call + 2u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.updateMultidevicePageContentData",
              call_data.arg1()->GetString());
    VerifyPageContent(call_data.arg2());
  }

  void CallRetryPendingHostSetup(bool success) {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("retryPendingHostSetup", &empty_args);
    fake_multidevice_setup_client()->InvokePendingRetrySetHostNowCallback(
        success);
  }

  void CallSetUpAndroidSms() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setUpAndroidSms", &empty_args);
  }

  void CallSetFeatureEnabledState(multidevice_setup::mojom::Feature feature,
                                  bool enabled,
                                  const base::Optional<std::string>& auth_token,
                                  bool success) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    args.AppendInteger(static_cast<int>(feature));
    args.AppendBoolean(enabled);
    if (auth_token)
      args.AppendString(*auth_token);

    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setFeatureEnabledState", &args);
    fake_multidevice_setup_client()
        ->InvokePendingSetFeatureEnabledStateCallback(
            feature /* expected_feature */, enabled /* expected_enabled */,
            auth_token /* expected_auth_token */, success);

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(success, call_data.arg3()->GetBool());
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *test_web_ui_->call_data()[index];
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return fake_multidevice_setup_client_.get();
  }

  android_sms::FakeAndroidSmsAppManager* fake_android_sms_app_manager() {
    return fake_android_sms_app_manager_.get();
  }

  phonehub::FakeNotificationAccessManager* fake_notification_access_manager() {
    return fake_notification_access_manager_.get();
  }

  void SimulateNotificationOptInStatusChange(
      phonehub::NotificationAccessSetupOperation::Status status) {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    fake_notification_access_manager()->SetNotificationSetupOperationStatus(
        status);

    bool completed_successfully = status ==
                                  phonehub::NotificationAccessSetupOperation::
                                      Status::kCompletedSuccessfully;
    if (completed_successfully)
      call_data_count_before_call++;

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());
    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    EXPECT_EQ("settings.onNotificationAccessSetupStatusChanged",
              call_data.arg1()->GetString());
    EXPECT_EQ(call_data.arg2()->GetInt(), static_cast<int32_t>(status));
  }

  bool IsNotificationAccessSetupOperationInProgress() {
    return fake_notification_access_manager()->IsSetupOperationInProgress();
  }

  const multidevice::RemoteDeviceRef test_device_;

 private:
  void VerifyPageContent(const base::Value* value) {
    VerifyPageContentDict(
        value, fake_multidevice_setup_client_->GetHostStatus().first,
        fake_multidevice_setup_client_->GetHostStatus().second,
        fake_multidevice_setup_client_->GetFeatureStates());
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<phonehub::FakeNotificationAccessManager>
      fake_notification_access_manager_;
  std::unique_ptr<multidevice_setup::FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device_;
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map_;
  std::unique_ptr<android_sms::FakeAndroidSmsAppManager>
      fake_android_sms_app_manager_;

  std::unique_ptr<TestMultideviceHandler> handler_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandlerTest);
};

TEST_F(MultideviceHandlerTest, NotificationSetupFlow) {
  using Status = phonehub::NotificationAccessSetupOperation::Status;

  // Simulate success flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(
      Status::kSentMessageToPhoneAndWaitingForResponse);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kCompletedSuccessfully);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate cancel flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  CallCancelNotificationSetup();
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate failure via time-out flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kTimedOutConnecting);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // Simulate failure via connected then disconnected flow.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/false);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnecting);
  EXPECT_TRUE(IsNotificationAccessSetupOperationInProgress());

  SimulateNotificationOptInStatusChange(Status::kConnectionDisconnected);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());

  // If access has already been granted, a setup operation should not occur.
  CallAttemptNotificationSetup(/*has_access_been_granted=*/true);
  EXPECT_FALSE(IsNotificationAccessSetupOperationInProgress());
}

TEST_F(MultideviceHandlerTest, PageContentData) {
  CallGetPageContentData();
  CallGetPageContentData();

  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      base::nullopt /* host_device */);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::
                               kHostSetLocallyButWaitingForBackendConfirmation,
                           test_device_);
  SimulateHostStatusUpdate(
      multidevice_setup::mojom::HostStatus::kHostSetButNotYetVerified,
      test_device_);
  SimulateHostStatusUpdate(multidevice_setup::mojom::HostStatus::kHostVerified,
                           test_device_);

  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();
  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kEnabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);

  feature_states_map[multidevice_setup::mojom::Feature::kBetterTogetherSuite] =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;
  SimulateFeatureStatesUpdate(feature_states_map);

  SimulatePairingStateUpdate(/*is_android_sms_pairing_complete=*/true);
}

TEST_F(MultideviceHandlerTest, RetryPendingHostSetup) {
  CallRetryPendingHostSetup(true /* success */);
  CallRetryPendingHostSetup(false /* success */);
}

TEST_F(MultideviceHandlerTest, SetUpAndroidSms) {
  EXPECT_FALSE(fake_android_sms_app_manager()->has_installed_app());
  EXPECT_FALSE(fake_android_sms_app_manager()->has_launched_app());
  CallSetUpAndroidSms();
  EXPECT_TRUE(fake_android_sms_app_manager()->has_installed_app());
  EXPECT_TRUE(fake_android_sms_app_manager()->has_launched_app());
}

TEST_F(MultideviceHandlerTest, SetFeatureEnabledState) {
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      true /* enabled */, "authToken" /* auth_token */, true /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, "authToken" /* auth_token */, false /* success */);
  CallSetFeatureEnabledState(
      multidevice_setup::mojom::Feature::kBetterTogetherSuite,
      false /* enabled */, "authToken" /* auth_token */, true /* success */);
}

TEST_F(MultideviceHandlerTest, RemoveHostDevice) {
  CallRemoveHostDevice();
  CallRemoveHostDevice();
  CallRemoveHostDevice();
}

TEST_F(MultideviceHandlerTest, GetAndroidSmsInfo) {
  // Check that getAndroidSmsInfo returns correct value.
  CallGetAndroidSmsInfo(false /* expected_enabled */,
                        android_sms::GetAndroidMessagesURL(
                            true /* use_install_url */) /* expected_url */);

  // Change messages feature state and assert that the change
  // callback is fired.
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
      feature_states_map = GenerateDefaultFeatureStatesMap();
  feature_states_map[multidevice_setup::mojom::Feature::kMessages] =
      multidevice_setup::mojom::FeatureState::kEnabledByUser;

  size_t call_data_count_before_call = test_web_ui()->call_data().size();
  SimulateFeatureStatesUpdate(feature_states_map);
  const content::TestWebUI::CallData& call_data_1 =
      CallDataAtIndex(call_data_count_before_call + 1);
  EXPECT_EQ("cr.webUIListenerCallback", call_data_1.function_name());
  EXPECT_EQ("settings.onAndroidSmsInfoChange", call_data_1.arg1()->GetString());

  // Check that getAndroidSmsInfo returns update value.
  CallGetAndroidSmsInfo(true /* enabled */, android_sms::GetAndroidMessagesURL(
                                                true) /* expected_url */);

  // Now, update the installed URL. This should have resulted in another call.
  fake_android_sms_app_manager()->SetInstalledAppUrl(
      android_sms::GetAndroidMessagesURL(true /* use_install_url */,
                                         android_sms::PwaDomain::kStaging));
  const content::TestWebUI::CallData& call_data_2 =
      CallDataAtIndex(call_data_count_before_call + 4);
  EXPECT_EQ("cr.webUIListenerCallback", call_data_2.function_name());
  EXPECT_EQ("settings.onAndroidSmsInfoChange", call_data_2.arg1()->GetString());
  CallGetAndroidSmsInfo(
      true /* enabled */,
      android_sms::GetAndroidMessagesURL(
          true /* use_install_url */,
          android_sms::PwaDomain::kStaging) /* expected_url */);
}

}  // namespace settings

}  // namespace chromeos
