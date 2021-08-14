// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class SafeBrowsingMetricsCollector;

// Singleton that owns SafeBrowsingMetricsCollector objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// object. It returns a nullptr in the Incognito mode.
class SafeBrowsingMetricsCollectorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Creates the object if it doesn't exist already for the given |profile|.
  // If the object already exists, return its pointer.
  static SafeBrowsingMetricsCollector* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static SafeBrowsingMetricsCollectorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      SafeBrowsingMetricsCollectorFactory>;

  SafeBrowsingMetricsCollectorFactory();
  ~SafeBrowsingMetricsCollectorFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingMetricsCollectorFactory);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
