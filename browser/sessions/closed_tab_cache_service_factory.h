// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/closed_tab_cache_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ClosedTabCacheServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ClosedTabCacheService for |profile|.
  static ClosedTabCacheService* GetForProfile(Profile* profile);

  static ClosedTabCacheServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ClosedTabCacheServiceFactory>;

  ClosedTabCacheServiceFactory();
  ~ClosedTabCacheServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_FACTORY_H_
