// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace first_run {

// Registers preferences related to ChromeOS first-run tutorial.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns true if we should launch the help app for the given Profile.
// Depends on user prefs and flags.
bool ShouldLaunchHelpApp(Profile* profile);

// Launches the help app for the given Profile. The app is preloaded
// immediately, but visible only after the session has begun.
void LaunchHelpApp(Profile* profile);

}  // namespace first_run
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_H_
