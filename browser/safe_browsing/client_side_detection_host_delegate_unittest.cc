// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionDelegateTest : public BrowserWithTestWindowTest {
 public:
  ClientSideDetectionDelegateTest() = default;
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://foo/0"));
    navigation_observer_manager_ = new SafeBrowsingNavigationObserverManager();
    navigation_observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        navigation_observer_manager_);
    scoped_feature_list_.InitAndEnableFeature(
        kClientSideDetectionReferrerChain);
  }

  void TearDown() override {
    delete navigation_observer_;
    BrowserWithTestWindowTest::TearDown();
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

 protected:
  SafeBrowsingNavigationObserverManager* navigation_observer_manager_;
  SafeBrowsingNavigationObserver* navigation_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionDelegateTest);
};

TEST_F(ClientSideDetectionDelegateTest, GetReferrerChain) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_hour_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://a.com/");
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  std::unique_ptr<ClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ClientSideDetectionHostDelegate>(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  csd_host_delegate->SetNavigationObserverManagerForTest(
      navigation_observer_manager_);
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"));
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(2, referrer_chain.size());

  EXPECT_EQ("http://b.com/", referrer_chain[0].url());
  EXPECT_EQ("http://a.com/", referrer_chain[0].referrer_url());
}

TEST_F(ClientSideDetectionDelegateTest, NoNavigationObserverManager) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_hour_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<ClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ClientSideDetectionHostDelegate>(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"));
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(0, referrer_chain.size());
}
}  // namespace safe_browsing
