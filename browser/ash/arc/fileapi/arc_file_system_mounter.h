// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Mounts/unmounts ARC file systems.
class ArcFileSystemMounter : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcFileSystemMounter* GetForBrowserContext(
      content::BrowserContext* context);

  ArcFileSystemMounter(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);
  ~ArcFileSystemMounter() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemMounter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_
