// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class Profile;

// Interface to allow the view delegate to call out to whatever is controlling
// the app list. This will have different implementations for different
// platforms.
class AppListControllerDelegate {
 public:
  // Whether apps can be pinned, and whether pinned apps are editable or fixed.
  // TODO(khmel): Find better home for Pinnable enum.
  enum Pinnable {
    NO_PIN,
    PIN_EDITABLE,
    PIN_FIXED
  };

  AppListControllerDelegate();
  virtual ~AppListControllerDelegate();

  // Dismisses the view.
  virtual void DismissView() = 0;

  // Gets app list window.
  virtual aura::Window* GetAppListWindow() = 0;

  // Gets display ID of app list window.
  virtual int64_t GetAppListDisplayId() = 0;

  // Control of pinning apps.
  virtual bool IsAppPinned(const std::string& app_id) = 0;
  virtual void PinApp(const std::string& app_id) = 0;
  virtual void UnpinApp(const std::string& app_id) = 0;
  virtual Pinnable GetPinnable(const std::string& app_id) = 0;

  // Returns true if requested app is open.
  virtual bool IsAppOpen(const std::string& app_id) const = 0;

  // Show the dialog with the application's information. Call only if
  // CanDoShowAppInfoFlow() returns true.
  virtual void DoShowAppInfoFlow(Profile* profile, const std::string& app_id);

  // Handle the "create window" context menu items of Chrome App.
  // |incognito| is true to create an incognito window.
  virtual void CreateNewWindow(bool incognito) = 0;

  // Opens the URL.
  virtual void OpenURL(Profile* profile,
                       const GURL& url,
                       ui::PageTransition transition,
                       WindowOpenDisposition disposition) = 0;

  // Uninstall the app identified by |app_id| from |profile|.
  void UninstallApp(Profile* profile, const std::string& app_id);

  // Shows the user the options page for the app.
  void ShowOptionsPage(Profile* profile, const std::string& app_id);

  // Gets/sets the launch type for an app.
  // The launch type specifies whether a hosted app should launch as a separate
  // window, fullscreened or as a tab.
  extensions::LaunchType GetExtensionLaunchType(Profile* profile,
                                                const std::string& app_id);
  virtual void SetExtensionLaunchType(Profile* profile,
                                      const std::string& extension_id,
                                      extensions::LaunchType launch_type);

  // Called when a search is started using the app list search box.
  void OnSearchStarted();

 private:
  base::WeakPtrFactory<AppListControllerDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_
