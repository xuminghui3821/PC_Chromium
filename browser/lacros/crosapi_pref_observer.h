// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CROSAPI_PREF_OBSERVER_H_
#define CHROME_BROWSER_LACROS_CROSAPI_PREF_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Helper to simplify the crosapi::mojom::PrefObserver API.
// Observes ash-chrome for changes in specified pref.
class CrosapiPrefObserver : public crosapi::mojom::PrefObserver {
 public:
  using PrefChangedCallback = base::RepeatingCallback<void(base::Value value)>;

  CrosapiPrefObserver(crosapi::mojom::PrefPath path,
                      PrefChangedCallback callback);
  CrosapiPrefObserver(const CrosapiPrefObserver&) = delete;
  CrosapiPrefObserver& operator=(const CrosapiPrefObserver&) = delete;
  ~CrosapiPrefObserver() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrosapiPrefObserverLacrosBrowserTest, Basics);

  // crosapi::mojom::PrefObserver:
  void OnPrefChanged(base::Value value) override;

  PrefChangedCallback callback_;

  // Receives mojo messages from ash.
  mojo::Receiver<crosapi::mojom::PrefObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_CROSAPI_PREF_OBSERVER_H_
