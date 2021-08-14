// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/performance_manager/public/performance_manager_main_thread_observer.h"

namespace performance_manager {

// PageLoadMetricsObserver records detailed metrics to explain what is included
// in the "Total Pageloads" presented on stability dashboards.
class PageLoadMetricsObserver
    : public PerformanceManagerMainThreadObserverDefaultImpl {
 public:
  PageLoadMetricsObserver();
  ~PageLoadMetricsObserver() override;
  PageLoadMetricsObserver(const PageLoadMetricsObserver& other) = delete;
  PageLoadMetricsObserver& operator=(const PageLoadMetricsObserver&) = delete;

  // PerformanceManagerMainThreadObserver:
  void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_
