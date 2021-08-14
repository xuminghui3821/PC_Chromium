// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
enum class SmsFetchFailureType;
}  // namespace content

// Manages remote sms fetching related communication between desktop and Android
// and the desktop UI around it.
class SmsRemoteFetcherUiController
    : public SharingUiController,
      public content::WebContentsUserData<SmsRemoteFetcherUiController> {
 public:
  using OnRemoteCallback =
      base::OnceCallback<void(base::Optional<std::vector<url::Origin>>,
                              base::Optional<std::string>,
                              base::Optional<content::SmsFetchFailureType>)>;
  static SmsRemoteFetcherUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);

  SmsRemoteFetcherUiController(const SmsRemoteFetcherUiController&) = delete;
  SmsRemoteFetcherUiController& operator=(const SmsRemoteFetcherUiController&) =
      delete;
  ~SmsRemoteFetcherUiController() override;

  // Overridden from SharingUiController:
  PageActionIconType GetIconType() override;
  sync_pb::SharingSpecificFields::EnabledFeatures GetRequiredFeature()
      const override;
  void OnDeviceChosen(const syncer::DeviceInfo& device) override;
  void OnAppChosen(const SharingApp& app) override;
  std::u16string GetContentType() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  SharingFeatureName GetFeatureMetricsPrefix() const override;

  void OnSmsRemoteFetchResponse(
      OnRemoteCallback callback,
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  base::OnceClosure FetchRemoteSms(const url::Origin& origin,
                                   OnRemoteCallback callback);

 protected:
  explicit SmsRemoteFetcherUiController(content::WebContents* web_contents);

  // Overridden from SharingUiController:
  void DoUpdateApps(UpdateAppsCallback callback) override;

 private:
  friend class content::WebContentsUserData<SmsRemoteFetcherUiController>;

  std::string last_device_name_;

  base::WeakPtrFactory<SmsRemoteFetcherUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_
