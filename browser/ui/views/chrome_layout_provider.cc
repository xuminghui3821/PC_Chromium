// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_layout_provider.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/shadow_value.h"

namespace {

// TODO(pbos): Inline kHarmonyLayoutUnit calculations below as it's not really
// respected (there's 3 * unit / 4 in use to express 12).
// The Harmony layout unit. All distances are in terms of this unit.
constexpr int kHarmonyLayoutUnit = 16;

ChromeLayoutProvider* g_chrome_layout_provider = nullptr;

}  // namespace

ChromeLayoutProvider::ChromeLayoutProvider() {
  DCHECK_EQ(nullptr, g_chrome_layout_provider);
  g_chrome_layout_provider = this;
}

ChromeLayoutProvider::~ChromeLayoutProvider() {
  DCHECK_EQ(this, g_chrome_layout_provider);
  g_chrome_layout_provider = nullptr;
}

// static
ChromeLayoutProvider* ChromeLayoutProvider::Get() {
  // Check to avoid downcasting a base LayoutProvider.
  DCHECK_EQ(g_chrome_layout_provider, views::LayoutProvider::Get());
  return static_cast<ChromeLayoutProvider*>(views::LayoutProvider::Get());
}

// static
std::unique_ptr<views::LayoutProvider>
ChromeLayoutProvider::CreateLayoutProvider() {
  return std::make_unique<ChromeLayoutProvider>();
}

gfx::Insets ChromeLayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_LT(metric, views::VIEWS_INSETS_MAX);
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (metric) {
    case views::INSETS_DIALOG:
    case views::INSETS_DIALOG_SUBSECTION:
      return gfx::Insets(kHarmonyLayoutUnit);
    case views::INSETS_CHECKBOX_RADIO_BUTTON: {
      gfx::Insets insets = LayoutProvider::GetInsetsMetric(metric);
      // Checkboxes and radio buttons should be aligned flush to the left edge.
      return gfx::Insets(insets.top(), 0, insets.bottom(), insets.right());
    }
    case views::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(kHarmonyLayoutUnit / 4);
    case views::InsetsMetric::INSETS_LABEL_BUTTON:
      return touch_ui
                 ? gfx::Insets(kHarmonyLayoutUnit / 2, kHarmonyLayoutUnit / 2)
                 : LayoutProvider::GetInsetsMetric(metric);
    case INSETS_BOOKMARKS_BAR_BUTTON:
      return touch_ui ? gfx::Insets(8, 10) : gfx::Insets(6);
    case INSETS_TOAST:
      return gfx::Insets(0, kHarmonyLayoutUnit);
    case INSETS_OMNIBOX_PILL_BUTTON:
      return touch_ui ? gfx::Insets(kHarmonyLayoutUnit / 2, kHarmonyLayoutUnit)
                      : gfx::Insets(5, 12);
    default:
      return LayoutProvider::GetInsetsMetric(metric);
  }
}

int ChromeLayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, views::VIEWS_DISTANCE_START);
  DCHECK_LT(metric, views::VIEWS_DISTANCE_MAX);

  if (metric < views::VIEWS_DISTANCE_END)
    return LayoutProvider::GetDistanceMetric(metric);

  switch (static_cast<ChromeDistanceMetric>(metric)) {
    case DISTANCE_CONTENT_LIST_VERTICAL_SINGLE:
      return kHarmonyLayoutUnit / 4;
    case DISTANCE_CONTENT_LIST_VERTICAL_MULTI:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return kHarmonyLayoutUnit * 3 / 4;
    case DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING:
      return 8;
    case DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN:
      return 12;
    case DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL:
      return kHarmonyLayoutUnit;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      return GetDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
    case DISTANCE_RELATED_LABEL_HORIZONTAL_LIST:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return 0;
    case DISTANCE_TOAST_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_TOAST_LABEL_VERTICAL:
      return 12;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return kHarmonyLayoutUnit;
    case DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE:
      return 20;
    case DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH:
      return kMediumDialogWidth;
    case DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH:
      return kLargeDialogWidth;
    case DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL:
      return 24;
    case DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING:
      return 8;
    case DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING:
      return 4;
  }
  NOTREACHED();
  return 0;
}

int ChromeLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  for (int snap_point :
       {kSmallDialogWidth, kMediumDialogWidth, kLargeDialogWidth}) {
    if (min_width <= snap_point)
      return snap_point;
  }

  return ((min_width + kHarmonyLayoutUnit - 1) / kHarmonyLayoutUnit) *
         kHarmonyLayoutUnit;
}

const views::TypographyProvider& ChromeLayoutProvider::GetTypographyProvider()
    const {
  return typography_provider_;
}

views::GridLayout::Alignment
ChromeLayoutProvider::GetControlLabelGridAlignment() const {
  return views::GridLayout::LEADING;
}

bool ChromeLayoutProvider::ShouldShowWindowIcon() const {
  return false;
}

gfx::ShadowValues ChromeLayoutProvider::MakeShadowValues(int elevation,
                                                         SkColor color) const {
  return gfx::ShadowValue::MakeRefreshShadowValues(elevation, color);
}