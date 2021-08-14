// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_internals_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/js/js_test_util.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using sync_pb::UserEventSpecifics;
using syncer::FakeUserEventService;
using syncer::SyncService;
using syncer::SyncServiceObserver;

namespace {

class TestableSyncInternalsMessageHandler : public SyncInternalsMessageHandler {
 public:
  TestableSyncInternalsMessageHandler(
      content::WebUI* web_ui,
      AboutSyncDataDelegate about_sync_data_delegate)
      : SyncInternalsMessageHandler(std::move(about_sync_data_delegate)) {
    set_web_ui(web_ui);
  }
};

class TestSyncService : public syncer::FakeSyncService {
 public:
  void AddObserver(SyncServiceObserver* observer) override {
    ++add_observer_count_;
  }

  void RemoveObserver(SyncServiceObserver* observer) override {
    ++remove_observer_count_;
  }

  base::WeakPtr<syncer::JsController> GetJsController() override {
    return js_controller_.AsWeakPtr();
  }

  void GetAllNodesForDebugging(
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback)
      override {
    get_all_nodes_callback_ = std::move(callback);
  }

  void GetEntityCountsForDebugging(
      base::OnceCallback<void(const std::vector<syncer::TypeEntitiesCount>&)>
          callback) const override {
    return std::move(callback).Run(
        {syncer::TypeEntitiesCount(syncer::PASSWORDS)});
  }

  int add_observer_count() const { return add_observer_count_; }
  int remove_observer_count() const { return remove_observer_count_; }
  base::OnceCallback<void(std::unique_ptr<base::ListValue>)>
  get_all_nodes_callback() {
    return std::move(get_all_nodes_callback_);
  }

 private:
  int add_observer_count_ = 0;
  int remove_observer_count_ = 0;
  syncer::MockJsController js_controller_;
  base::OnceCallback<void(std::unique_ptr<base::ListValue>)>
      get_all_nodes_callback_;
};

static std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>();
}

static std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return std::make_unique<FakeUserEventService>();
}

class SyncInternalsMessageHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  SyncInternalsMessageHandlerTest() = default;
  ~SyncInternalsMessageHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    about_information_.SetString("some_sync_state", "some_value");

    web_ui_.set_web_contents(web_contents());
    test_sync_service_ = static_cast<TestSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));
    fake_user_event_service_ = static_cast<FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(), base::BindRepeating(&BuildFakeUserEventService)));
    handler_ = std::make_unique<TestableSyncInternalsMessageHandler>(
        &web_ui_,
        base::BindRepeating(
            &SyncInternalsMessageHandlerTest::ConstructFakeAboutInformation,
            base::Unretained(this)));
  }

  void TearDown() override {
    // Destroy |handler_| before |web_contents()|.
    handler_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Returns copies of the same constant dictionary, |about_information_|.
  std::unique_ptr<DictionaryValue> ConstructFakeAboutInformation(
      SyncService* service,
      const std::string& channel) {
    ++about_sync_data_delegate_call_count_;
    last_delegate_sync_service_ = service;
    return base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(about_information_.Clone()));
  }

  void ValidateAboutInfoCall() {
    ASSERT_EQ(2u, web_ui_.call_data().size());

    // Check the syncer::sync_ui_util::kOnAboutInfoUpdated event dispatch.
    const content::TestWebUI::CallData& about_info_call_data =
        *web_ui_.call_data()[0];
    EXPECT_EQ("cr.webUIListenerCallback", about_info_call_data.function_name());
    ASSERT_NE(nullptr, about_info_call_data.arg1());
    EXPECT_EQ(base::Value(syncer::sync_ui_util::kOnAboutInfoUpdated),
              *about_info_call_data.arg1());
    ASSERT_NE(nullptr, about_info_call_data.arg2());
    EXPECT_EQ(about_information_, *about_info_call_data.arg2());

    // TestSyncService::GetEntityCountsForDebugging() responds synchronously,
    // so check the syncer::sync_ui_util::kOnEntityCountsUpdated event dispatch.
    const content::TestWebUI::CallData& entity_counts_updated_call_data =
        *web_ui_.call_data()[1];
    EXPECT_EQ("cr.webUIListenerCallback",
              entity_counts_updated_call_data.function_name());
    ASSERT_NE(nullptr, entity_counts_updated_call_data.arg1());
    EXPECT_EQ(base::Value(syncer::sync_ui_util::kOnEntityCountsUpdated),
              *entity_counts_updated_call_data.arg1());
  }

  void ValidateEmptyAboutInfoCall() {
    EXPECT_TRUE(web_ui_.call_data().empty());
  }

  TestSyncService* test_sync_service() { return test_sync_service_; }

  FakeUserEventService* fake_user_event_service() {
    return fake_user_event_service_;
  }

  TestableSyncInternalsMessageHandler* handler() { return handler_.get(); }

  int CallCountWithName(const std::string& function_name) {
    int count = 0;
    for (const auto& call_data : web_ui_.call_data()) {
      if (call_data->function_name() == function_name) {
        count++;
      }
    }
    return count;
  }

  int about_sync_data_delegate_call_count() const {
    return about_sync_data_delegate_call_count_;
  }

  const SyncService* last_delegate_sync_service() const {
    return last_delegate_sync_service_;
  }

  void ResetHandler() { handler_.reset(); }

 private:
  content::TestWebUI web_ui_;
  TestSyncService* test_sync_service_;
  FakeUserEventService* fake_user_event_service_;
  std::unique_ptr<TestableSyncInternalsMessageHandler> handler_;
  int about_sync_data_delegate_call_count_ = 0;
  SyncService* last_delegate_sync_service_ = nullptr;
  // Fake return value for sync_ui_util::ConstructAboutInformation().
  base::DictionaryValue about_information_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandlerTest);
};

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObservers) {
  ListValue empty_list;

  EXPECT_EQ(0, test_sync_service()->add_observer_count());
  handler()->HandleRequestDataAndRegisterForUpdates(&empty_list);
  EXPECT_EQ(1, test_sync_service()->add_observer_count());

  EXPECT_EQ(0, test_sync_service()->remove_observer_count());
  ResetHandler();
  EXPECT_EQ(1, test_sync_service()->remove_observer_count());

  // Add call should not have increased since the initial subscription.
  EXPECT_EQ(1, test_sync_service()->add_observer_count());
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversDisallowJavascript) {
  ListValue empty_list;

  EXPECT_EQ(0, test_sync_service()->add_observer_count());
  handler()->HandleRequestDataAndRegisterForUpdates(&empty_list);
  EXPECT_EQ(1, test_sync_service()->add_observer_count());

  EXPECT_EQ(0, test_sync_service()->remove_observer_count());
  handler()->DisallowJavascript();
  EXPECT_EQ(1, test_sync_service()->remove_observer_count());

  // Deregistration should not repeat, no counts should increase.
  ResetHandler();
  EXPECT_EQ(1, test_sync_service()->add_observer_count());
  EXPECT_EQ(1, test_sync_service()->remove_observer_count());
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversSyncDisabled) {
  // Simulate completely disabling sync by flag or other mechanism.
  ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  ListValue empty_list;
  handler()->HandleRequestDataAndRegisterForUpdates(&empty_list);
  handler()->DisallowJavascript();
  // Cannot verify observer methods on sync services were not called, because
  // there is no sync service. Rather, we're just making sure the handler hasn't
  // performed any invalid operations when the sync service is missing.
}

TEST_F(SyncInternalsMessageHandlerTest, HandleGetAllNodes) {
  ListValue args;
  args.AppendString("getAllNodes_0");
  handler()->HandleGetAllNodes(&args);
  test_sync_service()->get_all_nodes_callback().Run(
      std::make_unique<ListValue>());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  ListValue args2;
  args2.AppendString("getAllNodes_1");
  handler()->HandleGetAllNodes(&args2);
  // This  breaks the weak ref the callback is hanging onto. Which results in
  // the call count not incrementing.
  handler()->DisallowJavascript();
  test_sync_service()->get_all_nodes_callback().Run(
      std::make_unique<ListValue>());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  ListValue args3;
  args3.AppendString("getAllNodes_2");
  handler()->HandleGetAllNodes(&args3);
  test_sync_service()->get_all_nodes_callback().Run(
      std::make_unique<ListValue>());
  EXPECT_EQ(2, CallCountWithName("cr.webUIResponse"));
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfo) {
  handler()->AllowJavascriptForTesting();
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, about_sync_data_delegate_call_count());
  EXPECT_NE(nullptr, last_delegate_sync_service());
  ValidateAboutInfoCall();
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfoSyncDisabled) {
  // Simulate completely disabling sync by flag or other mechanism.
  ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  handler()->AllowJavascriptForTesting();
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, about_sync_data_delegate_call_count());
  EXPECT_EQ(nullptr, last_delegate_sync_service());
  ValidateAboutInfoCall();
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEvent) {
  ListValue args;
  args.AppendString("1000000000000000000");
  args.AppendString("-1");
  handler()->HandleWriteUserEvent(&args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(1000000000000000000, event.event_time_usec());
  EXPECT_EQ(-1, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBadParse) {
  ListValue args;
  args.AppendString("123abc");
  args.AppendString("abcdefghijklmnopqrstuvwxyz");
  handler()->HandleWriteUserEvent(&args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(0, event.event_time_usec());
  EXPECT_EQ(0, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBlank) {
  ListValue args;
  args.AppendString("");
  args.AppendString("");
  handler()->HandleWriteUserEvent(&args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should not have a navigation_id because that means something different to
  // the UserEvents logic.
  EXPECT_FALSE(event.has_navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventZero) {
  ListValue args;
  args.AppendString("0");
  args.AppendString("0");
  handler()->HandleWriteUserEvent(&args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should have a navigation_id, even though the value is 0.
  EXPECT_TRUE(event.has_navigation_id());
  EXPECT_EQ(0, event.navigation_id());
}

}  // namespace
