// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class ChromeEnterpriseRealTimeUrlLookupService;

// Singleton that owns ChromeEnterpriseRealTimeUrlLookupService objects, one for
// each active Profile. It listens to profile destroy events and destroy its
// associated service. It returns nullptr if the profile is in the Incognito
// mode.
class ChromeEnterpriseRealTimeUrlLookupServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static ChromeEnterpriseRealTimeUrlLookupService* GetForProfile(
      Profile* profile);

  // Get the singleton instance.
  static ChromeEnterpriseRealTimeUrlLookupServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ChromeEnterpriseRealTimeUrlLookupServiceFactory>;

  ChromeEnterpriseRealTimeUrlLookupServiceFactory();
  ~ChromeEnterpriseRealTimeUrlLookupServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeEnterpriseRealTimeUrlLookupServiceFactory);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
