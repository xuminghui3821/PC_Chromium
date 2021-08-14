// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/native_message_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"
#include "chrome/browser/ash/drive/drivefs_native_message_host.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_messaging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_message_built_in_host.h"
#include "chrome/browser/extensions/api/messaging/native_message_echo_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/host/it2me/it2me_native_messaging_host_allowed_origins.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"

namespace extensions {

namespace {

std::unique_ptr<NativeMessageHost> CreateIt2MeHost(
    content::BrowserContext* browser_context) {
  return remoting::CreateIt2MeNativeMessagingHostForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      g_browser_process->policy_service());
}

}  // namespace

const NativeMessageBuiltInHost kBuiltInHosts[] = {
    {NativeMessageEchoHost::kHostName, NativeMessageEchoHost::kOrigins,
     NativeMessageEchoHost::kOriginCount, &NativeMessageEchoHost::Create},
    {remoting::kIt2MeNativeMessageHostName, remoting::kIt2MeOrigins,
     remoting::kIt2MeOriginsSize, &CreateIt2MeHost},
    {arc::ArcSupportMessageHost::kHostName,
     arc::ArcSupportMessageHost::kHostOrigin, 1,
     &arc::ArcSupportMessageHost::Create},
    {chromeos::kWilcoDtcSupportdUiMessageHost,
     chromeos::kWilcoDtcSupportdHostOrigins,
     chromeos::kWilcoDtcSupportdHostOriginsSize,
     &chromeos::CreateExtensionOwnedWilcoDtcSupportdMessageHost},
    {drive::kDriveFsNativeMessageHostName,
     drive::kDriveFsNativeMessageHostOrigins,
     drive::kDriveFsNativeMessageHostOriginsSize,
     &drive::CreateDriveFsNativeMessageHost},
};

const size_t kBuiltInHostsCount = base::size(kBuiltInHosts);

}  // namespace extensions
