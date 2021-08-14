// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;

void TestingCallback(bool* out, base::OnceClosure closure, bool in) {
  *out = in;
  std::move(closure).Run();
}

namespace crostini {
using Protocol = CrostiniPortForwarder::Protocol;

class CrostiniPortForwarderTest : public testing::Test {
 public:
  CrostiniPortForwarderTest()
      : default_container_id_(ContainerId::GetDefault()),
        other_container_id_(ContainerId("other", "other")),
        inactive_container_id_(ContainerId("inactive", "inactive")) {}

  ~CrostiniPortForwarderTest() override {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::PermissionBrokerClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        kCrostiniDefaultVmName);
    CrostiniManager::GetForProfile(profile())->AddRunningContainerForTesting(
        kCrostiniDefaultVmName,
        ContainerInfo(kCrostiniDefaultContainerName, kCrostiniDefaultUsername,
                      "home/testuser1", "CONTAINER_IP_ADDRESS"));
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    crostini_port_forwarder_ =
        std::make_unique<CrostiniPortForwarder>(profile());
    crostini_port_forwarder_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    chromeos::PermissionBrokerClient::Shutdown();
    crostini_port_forwarder_->RemoveObserver(&mock_observer_);
    crostini_port_forwarder_.reset();
    test_helper_.reset();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  class MockPortObserver : public CrostiniPortForwarder::Observer {
   public:
    MOCK_METHOD(void,
                OnActivePortsChanged,
                (const base::ListValue& activePorts),
                (override));
  };

  Profile* profile() { return profile_.get(); }

  CrostiniPortForwarder::PortRuleKey GetPortKey(int port_number,
                                                Protocol protocol_type,
                                                ContainerId container_id) {
    return {
        .port_number = port_number,
        .protocol_type = protocol_type,
        .container_id = container_id,
    };
  }

  void MakePermissionBrokerPortForwardingExpectation(int port_number,
                                                     Protocol protocol,
                                                     bool exists,
                                                     const char* interface) {
    switch (protocol) {
      case Protocol::TCP:
        EXPECT_EQ(
            chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
                port_number, interface),
            exists);
        break;
      case Protocol::UDP:
        EXPECT_EQ(
            chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
                port_number, interface),
            exists);
        break;
    }
  }

  void MakePortPreferenceExpectation(CrostiniPortForwarder::PortRuleKey key,
                                     bool exists,
                                     std::string label) {
    base::Optional<base::Value> pref =
        crostini_port_forwarder_->ReadPortPreferenceForTesting(key);
    EXPECT_EQ(exists, pref.has_value());
    if (!exists) {
      return;
    }
    EXPECT_EQ(key.port_number,
              pref.value().FindIntKey(crostini::kPortNumberKey).value());
    EXPECT_EQ(static_cast<int>(key.protocol_type),
              pref.value().FindIntKey(crostini::kPortProtocolKey).value());
    EXPECT_EQ(key.container_id.vm_name,
              *pref.value().FindStringKey(crostini::kPortVmNameKey));
    EXPECT_EQ(key.container_id.container_name,
              *pref.value().FindStringKey(crostini::kPortContainerNameKey));
    EXPECT_EQ(label, *pref.value().FindStringKey(crostini::kPortLabelKey));
  }

  void MakePortExistenceExpectation(CrostiniPortForwarder::PortRuleKey port,
                                    std::string label,
                                    bool expected_pref,
                                    bool expected_permission) {
    MakePortPreferenceExpectation(port, /*exists=*/expected_pref,
                                  /*label=*/label);
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/expected_permission,
        /*interface=*/crostini::kDefaultInterfaceToForward);
  }

  bool AddPortFromKey(CrostiniPortForwarder::PortRuleKey port) {
    bool success = false;
    base::RunLoop run_loop;
    crostini_port_forwarder_->AddPort(
        port.container_id, port.port_number, port.protocol_type, "",
        base::BindOnce(&TestingCallback, base::Unretained(&success),
                       run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool ActivatePortFromKey(CrostiniPortForwarder::PortRuleKey port) {
    bool success = false;
    base::RunLoop run_loop;
    crostini_port_forwarder_->ActivatePort(
        port.container_id, port.port_number, port.protocol_type,
        base::BindOnce(&TestingCallback, base::Unretained(&success),
                       run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool RemovePortFromKey(CrostiniPortForwarder::PortRuleKey port) {
    bool success = false;
    base::RunLoop run_loop;
    crostini_port_forwarder_->RemovePort(
        port.container_id, port.port_number, port.protocol_type,
        base::BindOnce(&TestingCallback, base::Unretained(&success),
                       run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool DeactivatePortFromKey(CrostiniPortForwarder::PortRuleKey port) {
    bool success = false;
    base::RunLoop run_loop;
    crostini_port_forwarder_->DeactivatePort(
        port.container_id, port.port_number, port.protocol_type,
        base::BindOnce(&TestingCallback, base::Unretained(&success),
                       run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  ContainerId default_container_id_;
  ContainerId other_container_id_;
  ContainerId inactive_container_id_;

  testing::NiceMock<MockPortObserver> mock_observer_;

  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniPortForwarder> crostini_port_forwarder_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniPortForwarderTest);
};

TEST_F(CrostiniPortForwarderTest, AddPort) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(ports_to_add.size());

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Adding ports fails as they already exist.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_FALSE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);
}

TEST_F(CrostiniPortForwarderTest, RemovePort) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_remove = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> missing_ports_to_remove = {
      GetPortKey(5005, Protocol::TCP, default_container_id_),
      GetPortKey(5006, Protocol::UDP, default_container_id_)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_to_add.size() + ports_to_remove.size());

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Remove ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_remove) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_TRUE(RemovePortFromKey(port));
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);

  // Removing ports fails due to them not existing in prefs.
  for (CrostiniPortForwarder::PortRuleKey& port : missing_ports_to_remove) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_FALSE(RemovePortFromKey(port));
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
}

TEST_F(CrostiniPortForwarderTest, DeactivatePort) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_deactivate = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> missing_ports_to_deactivate =
      {GetPortKey(5005, Protocol::TCP, default_container_id_),
       GetPortKey(5006, Protocol::UDP, default_container_id_)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_to_add.size() + ports_to_deactivate.size() * 2);

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Deactivate ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_deactivate) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_TRUE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);

  // Deactivating ports fail due to the ports already being deactivated.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_deactivate) {
    MakePortExistenceExpectation(port, "", true, false);
    EXPECT_FALSE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);

  // Deactivating ports fails due to the ports not existing in the prefs.
  for (CrostiniPortForwarder::PortRuleKey& port : missing_ports_to_deactivate) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_FALSE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
}

TEST_F(CrostiniPortForwarderTest, ActivatePort) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_deactivate = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_activate = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> missing_ports_to_activate = {
      GetPortKey(5005, Protocol::TCP, default_container_id_),
      GetPortKey(5006, Protocol::TCP, default_container_id_),
      GetPortKey(5007, Protocol::TCP, default_container_id_)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_to_add.size() + ports_to_deactivate.size() +
             ports_to_activate.size());

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Deactivate ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_deactivate) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_TRUE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Activate ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_activate) {
    MakePortExistenceExpectation(port, "", true, false);
    EXPECT_TRUE(ActivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);

  // Activating ports fails due to ports already being active.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_activate) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_FALSE(ActivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);

  // Activating ports fails due to missing prefs.
  for (CrostiniPortForwarder::PortRuleKey& port : missing_ports_to_activate) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_FALSE(ActivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);
}

TEST_F(CrostiniPortForwarderTest, InactiveContainerHandling) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_for_inactive_container =
      {GetPortKey(5000, Protocol::TCP, inactive_container_id_),
       GetPortKey(5000, Protocol::UDP, inactive_container_id_),
       GetPortKey(5001, Protocol::UDP, inactive_container_id_)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_for_inactive_container.size() * 4);

  // Add ports, fails due to an inactive container.
  for (CrostiniPortForwarder::PortRuleKey& port :
       ports_for_inactive_container) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_FALSE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Activate ports, fails due to an inactive container.
  for (CrostiniPortForwarder::PortRuleKey& port :
       ports_for_inactive_container) {
    MakePortExistenceExpectation(port, "", true, false);
    EXPECT_FALSE(ActivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Deactivate ports, fails due to an inactive container.
  for (CrostiniPortForwarder::PortRuleKey& port :
       ports_for_inactive_container) {
    MakePortExistenceExpectation(port, "", true, false);
    EXPECT_FALSE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Remove ports, fails due to an inactive container.
  for (CrostiniPortForwarder::PortRuleKey& port :
       ports_for_inactive_container) {
    MakePortExistenceExpectation(port, "", true, false);
    EXPECT_FALSE(RemovePortFromKey(port));
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, DeactivateAllPorts) {
  ContainerId container_id = default_container_id_;
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, container_id),
      GetPortKey(5000, Protocol::UDP, container_id),
      GetPortKey(5001, Protocol::UDP, container_id)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_to_add.size() + 2);

  crostini_port_forwarder_->DeactivateAllActivePorts(container_id);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Deactivate all ports.
  crostini_port_forwarder_->DeactivateAllActivePorts(container_id);
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, RemoveAllPorts) {
  ContainerId container_id = default_container_id_;
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, container_id),
      GetPortKey(5000, Protocol::UDP, container_id),
      GetPortKey(5001, Protocol::UDP, container_id)};
  EXPECT_CALL(mock_observer_, OnActivePortsChanged)
      .Times(ports_to_add.size() + 2);

  // Remove all ports (ensuring that things don't break when there are
  // no ports to remove).
  crostini_port_forwarder_->RemoveAllPorts(container_id);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Remove all ports.
  crostini_port_forwarder_->RemoveAllPorts(container_id);
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, GetActivePorts) {
  ContainerId container_id = default_container_id_;
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, container_id),
      GetPortKey(5000, Protocol::UDP, container_id),
      GetPortKey(5001, Protocol::UDP, container_id)};

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Get active ports.
  base::ListValue forwarded_ports = crostini_port_forwarder_->GetActivePorts();
  EXPECT_EQ(forwarded_ports.GetList().size(), ports_to_add.size());
  for (unsigned int i = 0; i < ports_to_add.size(); i++) {
    unsigned int reverse_index = ports_to_add.size() - i - 1;
    EXPECT_EQ(*(forwarded_ports.GetList()[i].FindPath("port_number")),
              base::Value(ports_to_add.at(reverse_index).port_number));
    EXPECT_EQ(*(forwarded_ports.GetList()[i].FindPath("protocol_type")),
              base::Value(static_cast<int>(
                  ports_to_add.at(reverse_index).protocol_type)));
  }
}

TEST_F(CrostiniPortForwarderTest, ActiveNetworksChanged) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  const char eth_interface[] = "eth0";
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(ports_to_add.size());

  // Add ports
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortPreferenceExpectation(port, /*exists=*/true,
                                  /*label=*/"");
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/true, /*interface=*/crostini::kDefaultInterfaceToForward);
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/false, /*interface=*/eth_interface);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Request to update interface to kDefaultInterfaceToForward, no change
  // required as ports are already being forwarded on kDefaultInterfaceToForward
  // by default.
  crostini_port_forwarder_->ActiveNetworksChanged(
      crostini::kDefaultInterfaceToForward);
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortPreferenceExpectation(port, /*exists=*/true,
                                  /*label=*/"");
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/true, /*interface=*/crostini::kDefaultInterfaceToForward);
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/false, /*interface=*/eth_interface);
  }

  // Request to update interface to "", invalid request, no change required.
  crostini_port_forwarder_->ActiveNetworksChanged("");
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortPreferenceExpectation(port, /*exists=*/true,
                                  /*label=*/"");
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/true, /*interface=*/crostini::kDefaultInterfaceToForward);
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/false, /*interface=*/eth_interface);
  }

  // Request to update interface to eth_interface, ports are updated to use
  // the eth_interface and no longer use what they were using before
  // (kDefaultInterfaceToForward).
  crostini_port_forwarder_->ActiveNetworksChanged(eth_interface);
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortPreferenceExpectation(port, /*exists=*/true,
                                  /*label=*/"");
    // Deactivating forwarding on the previous interface is handled in Chromeos
    // and by the lifelines used to track port rules. Until the port is released
    // in Chromeos, both interfaces will be used.
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/true, /*interface=*/crostini::kDefaultInterfaceToForward);
    MakePermissionBrokerPortForwardingExpectation(
        /*port_number=*/port.port_number, /*protocol=*/port.protocol_type,
        /*exists=*/true, /*interface=*/eth_interface);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);
}

TEST_F(CrostiniPortForwarderTest, HandlingOfflinePermissionBroker) {
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_add = {
      GetPortKey(5000, Protocol::TCP, default_container_id_),
      GetPortKey(5000, Protocol::UDP, default_container_id_),
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  std::vector<CrostiniPortForwarder::PortRuleKey> ports_to_deactivate = {
      GetPortKey(5001, Protocol::UDP, default_container_id_)};
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Add ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    MakePortExistenceExpectation(port, "", false, false);
    EXPECT_TRUE(AddPortFromKey(port));
    MakePortExistenceExpectation(port, "", true, true);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  // Deactivate ports.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_deactivate) {
    MakePortExistenceExpectation(port, "", true, true);
    EXPECT_TRUE(DeactivatePortFromKey(port));
    MakePortExistenceExpectation(port, "", true, false);
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);

  // Shut PermissionBrokerClient down.
  chromeos::PermissionBrokerClient::Shutdown();

  // Activating ports fails, due to permission broker being inaccessible.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    EXPECT_FALSE(ActivatePortFromKey(port));
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);

  // Deactivating ports fails, due to permission broker being inaccessible.
  for (CrostiniPortForwarder::PortRuleKey& port : ports_to_add) {
    EXPECT_FALSE(DeactivatePortFromKey(port));
  }
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);

  // Re-initialize otherwise Shutdown in TearDown phase will break.
  chromeos::PermissionBrokerClient::InitializeFake();
}
}  // namespace crostini
