// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_
#define CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_

#include <memory>

#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

// Responsible for observing navigations that were typed in the omnibox
// and defaulted to HTTPS scheme and falling back to HTTP version when needed.
class TypedNavigationUpgradeThrottle : public content::NavigationThrottle {
 public:
  // Recorded in histograms. Do not reorder or delete values, only append.
  enum class Event {
    kNone = 0,
    // Started the load of an upgraded HTTPS URL.
    kHttpsLoadStarted,
    // Successfully finished loading the upgraded HTTPS URL.
    kHttpsLoadSucceeded,
    // Failed to load the upgraded HTTPS URL because of a cert error, fell back
    // to the HTTP URL.
    kHttpsLoadFailedWithCertError,
    // Failed to load the upgraded HTTPS URL because of a net error, fell back
    // to the HTTP URL.
    kHttpsLoadFailedWithNetError,
    // Failed to load the upgraded HTTPS URL within the timeout window, fell
    // back to the HTTP URL.
    kHttpsLoadTimedOut,
    // Received a redirect. This doesn't necessarily imply that the HTTPS load
    // succeeded or failed.
    kRedirected,
    kMaxValue = kRedirected,
  };

  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  ~TypedNavigationUpgradeThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  // Returns true if an SSL error with this navigation handle should not result
  // in an interstitial because the HTTPS load will fall back to HTTP on
  // failure.
  static bool ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps(
      content::NavigationHandle* handle);

  // Sets the port used by the embedded https server. This is used to determine
  // the correct port while upgrading URLs to https if the original URL has a
  // non-default port.
  static void SetHttpsPortForTesting(int https_port_for_testing);
  // Sets the port used by the embedded http server. This is used to determine
  // the correct port while falling back to http if the upgraded https URL has a
  // non-default port.
  static void SetHttpPortForTesting(int http_port_for_testing);
  static int GetHttpsPortForTesting();

  static const char kHistogramName[];

 private:
  explicit TypedNavigationUpgradeThrottle(content::NavigationHandle* handle);

  TypedNavigationUpgradeThrottle(const TypedNavigationUpgradeThrottle&) =
      delete;
  TypedNavigationUpgradeThrottle& operator=(
      const TypedNavigationUpgradeThrottle&) = delete;

  void OnHttpsLoadTimeout();

  // Initiates a new navigation to the HTTP version of the original navigation's
  // URL. If |stop_navigation| is true, also stops any pending navigation in the
  // current WebContents.
  void FallbackToHttp(bool stop_navigation);

  const GURL http_url_;
  base::OneShotTimer timer_;
  base::ElapsedTimer metrics_timer_;
};

#endif  // CHROME_BROWSER_SSL_TYPED_NAVIGATION_UPGRADE_THROTTLE_H_
