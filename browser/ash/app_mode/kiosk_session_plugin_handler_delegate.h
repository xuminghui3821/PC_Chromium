// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_

#include <set>

#include "base/files/file_path.h"

namespace ash {

class KioskSessionPluginHandlerDelegate {
 public:
  // Whether the plugin identified by the path should be handled.
  virtual bool ShouldHandlePlugin(const base::FilePath& plugin_path) const = 0;

  // Invoked after a plugin is crashed.
  virtual void OnPluginCrashed(const base::FilePath& plugin_path) = 0;

  // Invoked after plugins are hung.
  virtual void OnPluginHung(const std::set<int>& hung_plugins) = 0;

 protected:
  virtual ~KioskSessionPluginHandlerDelegate() {}
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_
