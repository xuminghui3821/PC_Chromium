// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

// Implementation of site_engagement::mojom::SiteEngagementDetailsProvider that
// gets information from the site_engagement::SiteEngagementService to provide
// data for the WebUI.
class SiteEngagementDetailsProviderImpl
    : public site_engagement::mojom::SiteEngagementDetailsProvider {
 public:
  // Instance is deleted when the supplied pipe is destroyed.
  SiteEngagementDetailsProviderImpl(
      Profile* profile,
      mojo::PendingReceiver<
          site_engagement::mojom::SiteEngagementDetailsProvider> receiver)
      : profile_(profile), receiver_(this, std::move(receiver)) {
    DCHECK(profile_);
  }

  ~SiteEngagementDetailsProviderImpl() override {}

  // site_engagement::mojom::SiteEngagementDetailsProvider overrides:
  void GetSiteEngagementDetails(
      GetSiteEngagementDetailsCallback callback) override {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile_);
    std::vector<site_engagement::mojom::SiteEngagementDetails> scores =
        service->GetAllDetails();

    std::vector<site_engagement::mojom::SiteEngagementDetailsPtr>
        engagement_info;
    engagement_info.reserve(scores.size());
    for (const auto& info : scores) {
      site_engagement::mojom::SiteEngagementDetailsPtr origin_info(
          site_engagement::mojom::SiteEngagementDetails::New());
      *origin_info = std::move(info);
      engagement_info.push_back(std::move(origin_info));
    }

    std::move(callback).Run(std::move(engagement_info));
  }

  void SetSiteEngagementBaseScoreForUrl(const GURL& origin,
                                        double score) override {
    if (!origin.is_valid() || score < 0 ||
        score > site_engagement::SiteEngagementService::GetMaxPoints() ||
        std::isnan(score)) {
      return;
    }

    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile_);
    service->ResetBaseScoreForURL(origin, score);
  }

 private:
  // The Profile* handed to us in our constructor.
  Profile* profile_;

  mojo::Receiver<site_engagement::mojom::SiteEngagementDetailsProvider>
      receiver_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementDetailsProviderImpl);
};

}  // namespace

SiteEngagementUI::SiteEngagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://site-engagement/ source.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUISiteEngagementHost));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->AddResourcePath("site_engagement.js", IDR_SITE_ENGAGEMENT_JS);
  source->AddResourcePath("site_engagement_details.mojom-webui.js",
                          IDR_SITE_ENGAGEMENT_DETAILS_MOJOM_WEBUI_JS);
  source->SetDefaultResource(IDR_SITE_ENGAGEMENT_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
}

WEB_UI_CONTROLLER_TYPE_IMPL(SiteEngagementUI)

SiteEngagementUI::~SiteEngagementUI() {}

void SiteEngagementUI::BindInterface(
    mojo::PendingReceiver<site_engagement::mojom::SiteEngagementDetailsProvider>
        receiver) {
  ui_handler_ = std::make_unique<SiteEngagementDetailsProviderImpl>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
