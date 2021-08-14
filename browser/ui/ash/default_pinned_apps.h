// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_H_
#define CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_H_

#include "base/containers/span.h"

using StaticAppId = const char* const;
base::span<StaticAppId> GetDefaultPinnedApps();
base::span<StaticAppId> GetTabletFormFactorDefaultPinnedApps();

#endif  // CHROME_BROWSER_UI_ASH_DEFAULT_PINNED_APPS_H_
