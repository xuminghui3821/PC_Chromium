// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace federated_learning {
class FlocRemotePermissionService;
}

// Used for creating and fetching a per-profile instance of the
// FlocRemotePermissionService.
class FlocRemotePermissionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Get the singleton instance of the factory.
  static FlocRemotePermissionServiceFactory* GetInstance();

  // Get the FlocRemotePermissionService for |profile|, creating one if needed.
  static federated_learning::FlocRemotePermissionService* GetForProfile(
      Profile* profile);

 protected:
  // Overridden from BrowserContextKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

 private:
  friend struct base::DefaultSingletonTraits<
      FlocRemotePermissionServiceFactory>;

  FlocRemotePermissionServiceFactory();
  ~FlocRemotePermissionServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(FlocRemotePermissionServiceFactory);
};

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_REMOTE_PERMISSION_SERVICE_FACTORY_H_
