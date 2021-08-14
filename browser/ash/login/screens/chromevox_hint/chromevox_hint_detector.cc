// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/idle_detector.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace chromeos {

namespace {
// Amount of time the user has to be idle for before giving the ChromeVox hint.
const base::TimeDelta kChromeVoxHintIdleDuration =
    base::TimeDelta::FromSeconds(20);
}  // namespace

ChromeVoxHintDetector::ChromeVoxHintDetector(const base::TickClock* clock,
                                             Observer* observer)
    : tick_clock_(clock), observer_(observer) {
  DCHECK(observer_);
  StartIdleDetection();
}

ChromeVoxHintDetector::~ChromeVoxHintDetector() {}

void ChromeVoxHintDetector::StartIdleDetection() {
  if (!features::IsOobeChromeVoxHintEnabled() ||
      chromeos::switches::IsOOBEChromeVoxHintTimerDisabledForTesting()) {
    return;
  }

  // This is done so that developers and testers don't repeatedly receive
  // the hint when flashing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSystemDevMode) &&
      !chromeos::switches::IsOOBEChromeVoxHintEnabledForDevMode()) {
    return;
  }

  // Only start the idle detector once.
  if (idle_detector_)
    return;

  auto callback = base::BindRepeating(&ChromeVoxHintDetector::OnIdle,
                                      weak_ptr_factory_.GetWeakPtr());
  idle_detector_ =
      std::make_unique<IdleDetector>(std::move(callback), tick_clock_);
  idle_detector_->Start(kChromeVoxHintIdleDuration);
}

void ChromeVoxHintDetector::OnIdle() {
  if (chromevox_hint_given_)
    return;

  chromevox_hint_given_ = true;
  observer_->OnShouldGiveChromeVoxHint();
}

}  // namespace chromeos
