// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_

#include <stddef.h>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"

namespace content {
class WebContents;
}

// SessionRestoreStatsCollector observes SessionRestore events ands records UMA
// accordingly.
//
// TODO(chrisha): Many of these metrics don't make sense to collect in the
// presence of an unavailable network, or when tabs are closed during loading.
// Rethink the collection in these cases.
class SessionRestoreStatsCollector : public content::RenderWidgetHostObserver {
 public:
  // Recorded in SessionRestore.ForegroundTabFirstPaint4.FinishReason metric.
  // Values other than PAINT_FINISHED_UMA_DONE indicate why FirstPaint time
  // was not recorded.
  enum SessionRestorePaintFinishReasonUma {
    // SessionRestore.ForegroundTabFirstPaint4_XX successfully recorded.
    PAINT_FINISHED_UMA_DONE = 0,
    // No tabs were visible the whole time before first paint.
    PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS = 1,
    // No restored tabs were painted.
    PAINT_FINISHED_UMA_NO_PAINT = 2,
    // A non-restored tab was painted first.
    PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST = 3,
    // The size of this enum. Must be the last entry.
    PAINT_FINISHED_UMA_MAX = 4,
  };

  // Houses all of the statistics gathered by the SessionRestoreStatsCollector
  // while the underlying TabLoader is active. These statistics are all reported
  // at once via the reporting delegate.
  struct TabLoaderStats {
    // Constructor that initializes everything to zero.
    TabLoaderStats();

    // The number of tabs involved in all overlapping session restores being
    // tracked by this SessionRestoreStatsCollector. This is used as suffix for
    // the "SessionRestore.ForegroundTabFirstPaint4" histogram.
    size_t tab_count;

    // The time elapsed between |restore_started| and reception of the first
    // NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES event for
    // any of the tabs involved in the session restore. If this is zero it is
    // because it has not been recorded (all restored tabs were closed or
    // hidden before they were painted, or were never painted). Corresponds to
    // "SessionRestore.ForegroundTabFirstPaint4" and its _XX variants.
    base::TimeDelta foreground_tab_first_paint;

    // Whether we recorded |foreground_tab_first_paint| and if not, why.
    SessionRestorePaintFinishReasonUma tab_first_paint_reason;
  };

  // The StatsReportingDelegate is responsible for delivering statistics
  // reported by the SessionRestoreStatsCollector.
  class StatsReportingDelegate;

  // An implementation of StatsReportingDelegate for reporting via UMA.
  class UmaStatsReportingDelegate;

  // Gets or creates an instance of SessionRestoreStatsCollector. An instance
  // self-deletes once it has reported all stats. If an existing instance is
  // returned, |restored_started| and |reporting_delegate| are ignored.
  static SessionRestoreStatsCollector* GetOrCreateInstance(
      base::TimeTicks restore_started,
      std::unique_ptr<StatsReportingDelegate> reporting_delegate);

  // Tracks stats for restored tabs. Tabs from overlapping session restores can
  // be tracked by the same SessionRestoreStatsCollector.
  void TrackTabs(const std::vector<SessionRestoreDelegate::RestoredTab>& tabs);

 private:
  friend class SessionRestoreStatsCollectorTest;

  // Constructs a SessionRestoreStatsCollector.
  SessionRestoreStatsCollector(
      const base::TimeTicks& restore_started,
      std::unique_ptr<StatsReportingDelegate> reporting_delegate);

  ~SessionRestoreStatsCollector() override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDidUpdateVisualProperties(
      content::RenderWidgetHost* widget_host) override;
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // Registers observers for a tab and inserts the tab into
  // |tracked_tabs_occluded_maps| map.
  void RegisterObserverForTab(content::WebContents* tab);

  // Report stats and self-deletes.
  void ReportStatsAndSelfDestroy();

  // Won't record time for foreground tab paint because a non-restored
  // tab was painted first.
  bool non_restored_tab_painted_first_;

  // Got first paint of tab that was hidden or occluded before being painted.
  bool hidden_or_occluded_tab_ignored_;

  // The time the restore process started.
  const base::TimeTicks restore_started_;

  // Tabs we are tracking paints in, keyed by their RenderWidgetHost and mapped
  // to a bool indicating whether the tab was ever hidden or occluded.
  std::map<content::RenderWidgetHost*, bool> tracked_tabs_occluded_map_;

  // Statistics gathered regarding the TabLoader.
  TabLoaderStats tab_loader_stats_;

  // The reporting delegate used to report gathered statistics.
  std::unique_ptr<StatsReportingDelegate> reporting_delegate_;

  base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                     content::RenderWidgetHostObserver>
      render_widget_host_observations_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreStatsCollector);
};

// An abstract reporting delegate is used as a testing seam.
class SessionRestoreStatsCollector::StatsReportingDelegate {
 public:
  StatsReportingDelegate() {}
  virtual ~StatsReportingDelegate() {}

  // Called when TabLoader has completed its work.
  virtual void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) = 0;

  // Called when a tab starts being tracked. Logs the relative time since last
  // use of the tab.
  virtual void ReportTabTimeSinceActive(base::TimeDelta elapsed) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatsReportingDelegate);
};

// The default reporting delegate, which reports statistics via UMA.
class SessionRestoreStatsCollector::UmaStatsReportingDelegate
    : public StatsReportingDelegate {
 public:
  UmaStatsReportingDelegate();
  ~UmaStatsReportingDelegate() override {}

  // StatsReportingDelegate:
  void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) override;
  void ReportTabTimeSinceActive(base::TimeDelta elapsed) override;

 private:

  DISALLOW_COPY_AND_ASSIGN(UmaStatsReportingDelegate);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
