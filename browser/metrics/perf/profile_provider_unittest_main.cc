// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_suite.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/perf/collection_params.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/perf_events_collector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

const base::TimeDelta kPeriodicCollectionInterval =
    base::TimeDelta::FromHours(1);
const base::TimeDelta kMaxCollectionDelay = base::TimeDelta::FromSeconds(1);
// Use a 2-sec collection duration.
const base::TimeDelta kCollectionDuration = base::TimeDelta::FromSeconds(2);
// The timeout in waiting until collection done. 8 sec is a safe value far
// beyond the collection duration used.
const base::TimeDelta kCollectionDoneTimeout = base::TimeDelta::FromSeconds(8);

class TestPerfCollector : public PerfCollector {
 public:
  explicit TestPerfCollector(const CollectionParams& params) : PerfCollector() {
    // Override default collection params.
    collection_params() = params;
  }
  ~TestPerfCollector() override = default;

  using internal::MetricCollector::collection_params;
};

class TestMetricProvider : public MetricProvider {
 public:
  using MetricProvider::MetricProvider;
  ~TestMetricProvider() override = default;

  using MetricProvider::set_cache_updated_callback;
};

// Allows access to some private methods for testing.
class TestProfileProvider : public ProfileProvider {
 public:
  TestProfileProvider() {
    CollectionParams test_params;
    test_params.collection_duration = kCollectionDuration;
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.resume_from_suspend.max_collection_delay = kMaxCollectionDelay;
    test_params.restore_session.sampling_factor = 1;
    test_params.restore_session.max_collection_delay = kMaxCollectionDelay;
    test_params.periodic_interval = kPeriodicCollectionInterval;

    collectors_.clear();
    auto metric_provider = std::make_unique<TestMetricProvider>(
        std::make_unique<TestPerfCollector>(test_params), nullptr);
    metric_provider->set_cache_updated_callback(base::BindRepeating(
        &TestProfileProvider::OnProfileDone, base::Unretained(this)));

    collectors_.push_back(std::move(metric_provider));
  }

  void WaitUntilCollectionDone() {
    // Collection shouldn't be done when this method is called, or the test will
    // waste time in |run_loop_| for the duration of |timeout|.
    EXPECT_FALSE(collection_done());

    timeout_timer_.Start(FROM_HERE, kCollectionDoneTimeout,
                         base::BindLambdaForTesting([&]() {
                           // Collection is not done yet. Quit the run loop to
                           // fail the test.
                           run_loop_.Quit();
                         }));
    // The run loop returns when its Quit() is called either on collection done
    // or on timeout. Note that the second call of Quit() is a noop.
    run_loop_.Run();

    if (timeout_timer_.IsRunning())
      // Timer is still running: the run loop doesn't quit on timeout. Stop the
      // timer.
      timeout_timer_.Stop();
  }

  bool collection_done() { return collection_done_; }

  using ProfileProvider::collectors_;
  using ProfileProvider::OnJankStarted;
  using ProfileProvider::OnJankStopped;
  using ProfileProvider::OnSessionRestoreDone;
  using ProfileProvider::SuspendDone;

 private:
  void OnProfileDone() {
    collection_done_ = true;
    // Notify that profile collection is done. Quitting the run loop is
    // thread-safe.
    run_loop_.Quit();
  }

  base::OneShotTimer timeout_timer_;
  base::RunLoop run_loop_;
  bool collection_done_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestProfileProvider);
};

// This test doesn't mock any class used indirectly by ProfileProvider to make
// real collections from debugd.
class ProfileProviderRealCollectionTest : public testing::Test {
 public:
  ProfileProviderRealCollectionTest() {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();

    // The constructor of ProfileProvider uses g_browser_process thus requiring
    // it to be not null, so initialize it here.
    TestingBrowserProcess::CreateInstance();

    std::map<std::string, std::string> field_trial_params;
    // Only "cycles" event is supported.
    field_trial_params.insert(std::make_pair(
        "PerfCommand::default::0", "50 perf record -a -e cycles -c 1000003"));
    field_trial_params.insert(
        std::make_pair("PerfCommand::default::1",
                       "50 perf record -a -e cycles -g -c 4000037"));
    ASSERT_TRUE(variations::AssociateVariationParams(
        "ChromeOSWideProfilingCollection", "group_name", field_trial_params));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        "ChromeOSWideProfilingCollection", "group_name");
    ASSERT_TRUE(field_trial_.get());

    profile_provider_ = std::make_unique<TestProfileProvider>();
    profile_provider_->Init();

    // Set user state as logged in. This activates periodic collection, but
    // other triggers like SUSPEND_DONE take precedence.
    chromeos::LoginState::Get()->SetLoggedInState(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR);

    // Finishes Init() on the dedicated sequence.
    task_environment_.RunUntilIdle();

    StartSpinningCPU();
  }

  void TearDown() override {
    StopSpinningCPU();

    profile_provider_.reset();
    TestingBrowserProcess::DeleteInstance();
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    variations::testing::ClearAllVariationParams();
  }

  void AssertProfileData(SampledProfile::TriggerEvent trigger_event) {
    // Sets up a ScopedClosureRunner for logging extra information on assertion
    // failure.
    base::ScopedClosureRunner scoped_log_error(base::BindOnce([]() {
      // Collection failed: log the failure in the UMA histogram.
      auto* histogram =
          base::StatisticsRecorder::FindHistogram("ChromeOS.CWP.CollectPerf");
      if (!histogram) {
        LOG(WARNING) << "Profile collection failed without "
                        "ChromeOS.CWP.CollectPerf histogram data";
        return;
      }

      std::string histogram_ascii;
      histogram->WriteAscii(&histogram_ascii);
      LOG(ERROR) << "Profile collection result: " << histogram_ascii;
    }));

    std::vector<SampledProfile> stored_profiles;
    ASSERT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));

    auto& profile = stored_profiles[0];
    EXPECT_EQ(trigger_event, profile.trigger_event());

    ASSERT_TRUE(profile.has_perf_data());

    // Collection succeeded: don't output the error log.
    ignore_result(scoped_log_error.Release());
  }

 protected:
  // Spins the CPU to move forward the CPU cycles counter and makes sure the
  // perf session always has samples to collect.
  void StartSpinningCPU() {
    spin_cpu_ = true;
    spin_cpu_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    spin_cpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](ProfileProviderRealCollectionTest* self) {
                         while (self->spin_cpu_) {
                         }
                         // Signal that this task is exiting and won't touch
                         // |this| anymore.
                         self->spin_cpu_done_.Signal();
                       },
                       base::Unretained(this)));
  }

  void StopSpinningCPU() {
    spin_cpu_ = false;

    // Wait until the current sequence is signaled that the CPU spinning task
    // has finished execution so it doesn't use any data member of |this|.
    if (!spin_cpu_done_.IsSignaled())
      spin_cpu_done_.Wait();

    spin_cpu_task_runner_ = nullptr;
  }

  // |task_environment_| must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and destroyed
  // last.
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<base::FieldTrial> field_trial_;

  scoped_refptr<base::SequencedTaskRunner> spin_cpu_task_runner_;
  std::atomic_bool spin_cpu_{false};
  base::WaitableEvent spin_cpu_done_;

  std::unique_ptr<TestProfileProvider> profile_provider_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProviderRealCollectionTest);
};

// Flaky on chromeos: crbug.com/1184119
#if defined(OS_CHROMEOS)
#define MAYBE_SuspendDone DISABLED_SuspendDone
#else
#define MAYBE_SuspendDone SuspendDone
#endif
TEST_F(ProfileProviderRealCollectionTest, MAYBE_SuspendDone) {
  // Trigger a resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  AssertProfileData(SampledProfile::RESUME_FROM_SUSPEND);
}

TEST_F(ProfileProviderRealCollectionTest, SessionRestoreDone) {
  // Restored 10 tabs.
  profile_provider_->OnSessionRestoreDone(10);

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  AssertProfileData(SampledProfile::RESTORE_SESSION);
}

// Flaky on Chrome OS: crbug.com/1188498.
#if defined(OS_CHROMEOS)
#define MAYBE_OnJankStarted DISABLED_OnJankStarted
#else
#define MAYBE_OnJankStarted OnJankStarted
#endif
TEST_F(ProfileProviderRealCollectionTest, MAYBE_OnJankStarted) {
  // Trigger a resume from suspend.
  profile_provider_->OnJankStarted();

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  AssertProfileData(SampledProfile::JANKY_TASK);
}

// TODO(crbug.com/1177150) Re-enable test
TEST_F(ProfileProviderRealCollectionTest, DISABLED_OnJankStopped) {
  profile_provider_->OnJankStarted();

  // Call ProfileProvider::OnJankStopped() halfway through the collection
  // duration.
  base::OneShotTimer stop_timer;
  base::RunLoop run_loop;
  // The jank lasts for 0.75*(collection duration), which is 1.5 sec.
  stop_timer.Start(FROM_HERE, kCollectionDuration * 3 / 4,
                   base::BindLambdaForTesting([&]() {
                     profile_provider_->OnJankStopped();
                     run_loop.Quit();
                   }));
  run_loop.Run();
  // |run_loop| quits only by |stop_timer| so the timer shouldn't be running.
  EXPECT_FALSE(stop_timer.IsRunning());

  profile_provider_->WaitUntilCollectionDone();
  EXPECT_TRUE(profile_provider_->collection_done());

  AssertProfileData(SampledProfile::JANKY_TASK);
}

}  // namespace metrics

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  return base::RunUnitTestsUsingBaseTestSuite(argc, argv);
}