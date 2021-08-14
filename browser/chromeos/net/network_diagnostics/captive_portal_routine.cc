// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/captive_portal_routine.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

mojom::CaptivePortalProblem GetProblemFromPortalState(
    network_config::mojom::PortalState portal_state) {
  switch (portal_state) {
    case network_config::mojom::PortalState::kOnline:
      // Already handled, not expected, fall through.
      break;
    case network_config::mojom::PortalState::kUnknown:
      return mojom::CaptivePortalProblem::kUnknownPortalState;
    case network_config::mojom::PortalState::kPortalSuspected:
      return mojom::CaptivePortalProblem::kPortalSuspected;
    case network_config::mojom::PortalState::kPortal:
      return mojom::CaptivePortalProblem::kPortal;
    case network_config::mojom::PortalState::kProxyAuthRequired:
      return mojom::CaptivePortalProblem::kProxyAuthRequired;
    case network_config::mojom::PortalState::kNoInternet:
      return mojom::CaptivePortalProblem::kNoInternet;
  }
  NOTREACHED();
  return mojom::CaptivePortalProblem::kUnknownPortalState;
}

}  // namespace

CaptivePortalRoutine::CaptivePortalRoutine() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

CaptivePortalRoutine::~CaptivePortalRoutine() = default;

void CaptivePortalRoutine::RunRoutine(CaptivePortalRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveNetworks();
}

void CaptivePortalRoutine::AnalyzeResultsAndExecuteCallback() {
  if (no_active_networks_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::CaptivePortalProblem::kNoActiveNetworks);
  } else if (portal_state_ == network_config::mojom::PortalState::kOnline) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  } else {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(GetProblemFromPortalState(portal_state_));
  }
  std::move(routine_completed_callback_).Run(verdict(), problems_);
}

void CaptivePortalRoutine::FetchActiveNetworks() {
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&CaptivePortalRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void CaptivePortalRoutine::FetchManagedProperties(const std::string& guid) {
  remote_cros_network_config_->GetManagedProperties(
      guid, base::BindOnce(&CaptivePortalRoutine::OnManagedPropertiesReceived,
                           base::Unretained(this)));
}

void CaptivePortalRoutine::OnNetworkStateListReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  if (networks.size() == 0) {
    no_active_networks_ = true;
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  if (networks[0]->connection_state !=
      network_config::mojom::ConnectionStateType::kPortal) {
    // Not in a captive portal state.
    portal_state_ = network_config::mojom::PortalState::kOnline;
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  FetchManagedProperties(networks[0]->guid);
}

void CaptivePortalRoutine::OnManagedPropertiesReceived(
    network_config::mojom::ManagedPropertiesPtr managed_properties) {
  if (managed_properties) {
    portal_state_ = managed_properties->portal_state;
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace chromeos
