// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is included from autogenerated files based on
// chrome/common/extensions/api/enterprise_platform_keys_internal.idl and
// chrome/common/extensions/api/enterprise_platform_keys.idl.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api_lacros.h"
#else
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api_ash.h"
#endif

class Extension;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions {
namespace platform_keys {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

bool IsExtensionAllowed(Profile* profile, const Extension* extension);

}  // namespace platform_keys
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_H_
