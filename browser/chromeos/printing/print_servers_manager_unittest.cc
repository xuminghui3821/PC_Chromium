// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_servers_manager.h"

#include <map>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/print_servers_provider.h"
#include "chrome/browser/chromeos/printing/server_printers_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FakeServerPrintersProvider : public ServerPrintersProvider {
 public:
  FakeServerPrintersProvider() = default;
  ~FakeServerPrintersProvider() override = default;

  void RegisterPrintersFoundCallback(OnPrintersUpdateCallback cb) override {}

  void OnServersChanged(bool servers_are_complete,
                        const std::map<GURL, PrintServer>& servers) override {
    std::vector<chromeos::PrintServer> print_servers;
    for (auto& server_pair : servers) {
      print_servers.push_back(server_pair.second);
    }
    print_servers_ = print_servers;
  }

  std::vector<PrinterDetector::DetectedPrinter> GetPrinters() override {
    std::vector<PrinterDetector::DetectedPrinter> printers;
    return printers;
  }

  std::vector<chromeos::PrintServer> GetPrintServers() {
    return print_servers_;
  }

 private:
  std::vector<chromeos::PrintServer> print_servers_;
};

class FakePrintServersProvider : public PrintServersProvider {
 public:
  FakePrintServersProvider() = default;
  ~FakePrintServersProvider() override = default;

  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override {}
  void SetData(std::unique_ptr<std::string> data) override {}
  void SetAllowlistPref(PrefService* prefs,
                        const std::string& allowlist_pref) override {}
  void ClearData() override {}

  base::Optional<std::vector<PrintServer>> GetPrintServers() override {
    return print_servers_;
  }

  void SetPrintServers(base::Optional<std::vector<PrintServer>> print_servers) {
    print_servers_ = print_servers;
    if (observer_) {
      observer_->OnServersChanged(print_servers.has_value(),
                                  print_servers.value());
    }
  }

 private:
  base::Optional<std::vector<PrintServer>> print_servers_;
  PrintServersProvider::Observer* observer_;
};

class PrintServersManagerTest : public testing::Test,
                                public PrintServersManager::Observer {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPrintServerScaling}, {});
  }
  PrintServersManagerTest() {
    auto server_printers_provider =
        std::make_unique<FakeServerPrintersProvider>();
    server_printers_provider_ = server_printers_provider.get();
    auto print_servers_policy_provider =
        PrintServersPolicyProvider::CreateForTesting(
            user_policy_print_servers_provider_.AsWeakPtr(),
            device_policy_print_servers_provider_.AsWeakPtr());

    PrintServersManager::RegisterProfilePrefs(pref_service_.registry());

    manager_ = PrintServersManager::CreateForTesting(
        std::move(server_printers_provider),
        std::move(print_servers_policy_provider));
    manager_->AddObserver(this);
  }

  ~PrintServersManagerTest() override {}

  static chromeos::PrintServer CreatePrintServer(std::string id,
                                                 std::string server_url,
                                                 std::string name) {
    GURL url(server_url);
    chromeos::PrintServer print_server(id, url, name);
    return print_server;
  }

 protected:
  // Everything from PrintServersProvider must be called on Chrome_UIThread
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Captured printer lists from observer callbacks.
  base::flat_map<PrinterClass, std::vector<Printer>> observed_printers_;

  FakeServerPrintersProvider* server_printers_provider_;
  FakePrintServersProvider user_policy_print_servers_provider_;
  FakePrintServersProvider device_policy_print_servers_provider_;

  // PrefService used to register the |UserPrintersAllowed| pref and
  // change its value for testing.
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  // The manager being tested.
  std::unique_ptr<PrintServersManager> manager_;
};

TEST_F(PrintServersManagerTest, GetServerPrinters_StandardMode) {
  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  std::vector<chromeos::PrintServer> user_print_servers;
  auto user_print_server =
      CreatePrintServer("1", "http://192.168.1.5/user-printer", "LexaPrint");
  user_print_servers.push_back(user_print_server);
  user_policy_print_servers_provider_.SetPrintServers(user_print_servers);
  std::vector<chromeos::PrintServer> device_print_servers;
  auto device_print_server = CreatePrintServer(
      "2", "http://192.168.1.5/device-printer", "Color Laser");
  device_print_servers.push_back(device_print_server);
  device_policy_print_servers_provider_.SetPrintServers(device_print_servers);

  EXPECT_THAT(
      server_printers_provider_->GetPrintServers(),
      testing::UnorderedElementsAre(user_print_server, device_print_server));
}

TEST_F(PrintServersManagerTest, GetServerPrinters_SingleServerOnly) {
  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  auto selected_print_server =
      CreatePrintServer("user-1", "http://user-print-1", "User LexaPrint - 1");

  std::vector<chromeos::PrintServer> user_print_servers;
  for (int i = 1; i <= 10; ++i) {
    auto id = base::NumberToString(i);
    auto print_server = CreatePrintServer(
        "user-" + id, "http://user-print-" + id, "User LexaPrint - " + id);
    user_print_servers.push_back(print_server);
  }
  user_policy_print_servers_provider_.SetPrintServers(user_print_servers);
  std::vector<chromeos::PrintServer> device_print_servers;
  for (int i = 1; i <= 7; ++i) {
    auto id = base::NumberToString(i);
    auto print_server =
        CreatePrintServer("device-" + id, "http://device-print-" + id,
                          "Device LexaPrint - " + id);
    device_print_servers.push_back(print_server);
  }
  device_policy_print_servers_provider_.SetPrintServers(device_print_servers);

  std::vector<std::string> ids;
  ids.push_back(selected_print_server.GetId());
  manager_->ChoosePrintServer(ids);

  EXPECT_THAT(server_printers_provider_->GetPrintServers(),
              testing::UnorderedElementsAre(selected_print_server));
}

}  // namespace
}  // namespace chromeos
