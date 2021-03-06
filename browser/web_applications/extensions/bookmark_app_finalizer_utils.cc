// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_finalizer_utils.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"

namespace extensions {

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool CanOsAddDesktopShortcuts() {
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || defined(OS_WIN)
  return true;
#else
  return false;
#endif
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

bool CanBookmarkAppCreateOsShortcuts() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return true;
#endif
}

void BookmarkAppCreateOsShortcuts(
    Profile* profile,
    const Extension* extension,
    bool add_to_desktop,
    base::OnceCallback<void(bool created_shortcuts)> callback) {
  DCHECK(CanBookmarkAppCreateOsShortcuts());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  web_app::ShortcutLocations creation_locations;
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  creation_locations.in_quick_launch_bar = false;

  if (CanOsAddDesktopShortcuts())
    creation_locations.on_desktop = add_to_desktop;

  Profile* current_profile = profile->GetOriginalProfile();
  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                           creation_locations, current_profile, extension,
                           std::move(callback));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace extensions
