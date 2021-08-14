// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace chromeos {

namespace {

// The ChromeVox hint idle duration is 20s. We set this to 25s, since it's safer
// for this to be slightly longer than the true idle duration.
const base::TimeDelta kFullIdleDuration = base::TimeDelta::FromSeconds(25);
const base::TimeDelta kThreeFourthIdleDuration =
    kFullIdleDuration - kFullIdleDuration / 4;

class MockDetectorObserver : public ChromeVoxHintDetector::Observer {
 public:
  MockDetectorObserver() = default;
  virtual ~MockDetectorObserver() = default;

  MOCK_METHOD(void, OnShouldGiveChromeVoxHint, (), (override));
};

}  // namespace

class ChromeVoxHintDetectorTest : public testing::Test {
 protected:
  ChromeVoxHintDetectorTest();
  ~ChromeVoxHintDetectorTest() override = default;

  void StartDetection();
  void ExpectChromeVoxHintWillBeGiven();
  void ExpectChromeVoxHintWillNotBeGiven();
  void SimulateUserActivity();
  scoped_refptr<base::TestMockTimeTaskRunner> runner_;

 private:
  MockDetectorObserver observer_;
  std::unique_ptr<ChromeVoxHintDetector> detector_;
  std::unique_ptr<base::ThreadTaskRunnerHandle> runner_handle_;
  ui::UserActivityDetector user_activity_detector_;
};

ChromeVoxHintDetectorTest::ChromeVoxHintDetectorTest() {
  runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  runner_handle_ = std::make_unique<base::ThreadTaskRunnerHandle>(runner_);
}

void ChromeVoxHintDetectorTest::ExpectChromeVoxHintWillBeGiven() {
  EXPECT_CALL(observer_, OnShouldGiveChromeVoxHint());
}

void ChromeVoxHintDetectorTest::ExpectChromeVoxHintWillNotBeGiven() {
  EXPECT_CALL(observer_, OnShouldGiveChromeVoxHint()).Times(0);
}

void ChromeVoxHintDetectorTest::StartDetection() {
  detector_ = std::make_unique<ChromeVoxHintDetector>(
      runner_->GetMockTickClock(), &observer_);
}

void ChromeVoxHintDetectorTest::SimulateUserActivity() {
  user_activity_detector_.HandleExternalUserActivity();
}

// Tests that the ChromeVox hint is given after idling for the proper duration.
TEST_F(ChromeVoxHintDetectorTest, HintAfterIdleTimeout) {
  ExpectChromeVoxHintWillBeGiven();
  StartDetection();
  runner_->FastForwardBy(kFullIdleDuration);
}

// Tests that the ChromeVox hint will not be given before the idle timeout.
TEST_F(ChromeVoxHintDetectorTest, NoHintBeforeIdleTimeout) {
  ExpectChromeVoxHintWillNotBeGiven();
  StartDetection();
  runner_->FastForwardBy(kThreeFourthIdleDuration);
}

// Tests that user activity resets the detector and that no hint is given.
TEST_F(ChromeVoxHintDetectorTest, ResetOnUserActivityNoHint) {
  ExpectChromeVoxHintWillNotBeGiven();
  StartDetection();
  runner_->FastForwardBy(kThreeFourthIdleDuration);
  SimulateUserActivity();
  runner_->FastForwardBy(kThreeFourthIdleDuration);
}

// Tests that user activity resets the detector. Idling for the full duration
// after user activity should give the hint.
TEST_F(ChromeVoxHintDetectorTest, ResetOnUserActivityGiveHint) {
  ExpectChromeVoxHintWillBeGiven();
  StartDetection();
  runner_->FastForwardBy(kThreeFourthIdleDuration);
  SimulateUserActivity();
  runner_->FastForwardBy(kFullIdleDuration);
}

// Tests that the ChromeVox hint isn't given if the disabling switch is present.
TEST_F(ChromeVoxHintDetectorTest, NoHintWithDisablingSwitch) {
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kDisableOOBEChromeVoxHintTimerForTesting);
  ExpectChromeVoxHintWillNotBeGiven();
  StartDetection();
  runner_->FastForwardBy(kFullIdleDuration);
}

// Tests that the ChromeVox hint isn't given in dev mode.
TEST_F(ChromeVoxHintDetectorTest, NoHintInDevMode) {
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kSystemDevMode);
  ExpectChromeVoxHintWillNotBeGiven();
  StartDetection();
  runner_->FastForwardBy(kFullIdleDuration);
}

// Tests that the ChromeVox hint is given in dev mode if the enabling switch
// is present.
TEST_F(ChromeVoxHintDetectorTest, HintInDevModeWithEnablingSwitch) {
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kSystemDevMode);
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kEnableOOBEChromeVoxHintForDevMode);
  ExpectChromeVoxHintWillBeGiven();
  StartDetection();
  runner_->FastForwardBy(kFullIdleDuration);
}

// Tests that the disabling switch overrides the switch to enable the hint in
// dev mode.
TEST_F(ChromeVoxHintDetectorTest, NoHintWithDisablingSwitchInDevMode) {
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kSystemDevMode);
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kEnableOOBEChromeVoxHintForDevMode);
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kDisableOOBEChromeVoxHintTimerForTesting);
  ExpectChromeVoxHintWillNotBeGiven();
  StartDetection();
  runner_->FastForwardBy(kFullIdleDuration);
}

}  // namespace chromeos
