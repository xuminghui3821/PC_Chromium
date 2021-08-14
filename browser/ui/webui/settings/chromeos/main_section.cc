// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/main_section.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
      {"searchNoResults", IDS_SEARCH_NO_RESULTS},
      {"searchResults", IDS_SEARCH_RESULTS},
      {"searchResultSelected", IDS_OS_SEARCH_RESULT_ROW_A11Y_RESULT_SELECTED},
      {"clearSearch", IDS_CLEAR_SEARCH},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "searchNoOsResultsHelp",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
          base::ASCIIToUTF16(chrome::kOsSettingsSearchHelpURL)));

  // TODO(crbug/1080777): Remove this flag and JS codepaths effected.
  html_source->AddBoolean("newOsSettingsSearch", true);
}

void AddUpdateRequiredEolStrings(content::WebUIDataSource* html_source) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::MinimumVersionPolicyHandler* handler =
      connector->GetMinimumVersionPolicyHandler();
  bool device_managed = connector->IsEnterpriseManaged();

  // |eol_return_banner_text| contains the update required end of life banner
  // text which is left empty when the banner should not be shown.
  std::u16string eol_return_banner_text;
  if (device_managed && handler->ShouldShowUpdateRequiredEolBanner()) {
    base::Optional<int> days = handler->GetTimeRemainingInDays();
    // We only need to show the banner if less than equal to one week remains to
    // reach the update required deadline.
    if (days && days.value() <= 7) {
      // |days| could have value equal to zero if we are very close to the
      // deadline.
      int days_remaining = days.value() ? days.value() : 1;
      std::u16string domain_name =
          base::UTF8ToUTF16(connector->GetEnterpriseDomainManager());
      std::u16string link_url =
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL);
      if (days_remaining == 7) {
        eol_return_banner_text = l10n_util::GetStringFUTF16(
            IDS_SETTINGS_UPDATE_REQUIRED_EOL_BANNER_ONE_WEEK, domain_name,
            ui::GetChromeOSDeviceName(), link_url);
      } else {
        eol_return_banner_text =
            base::i18n::MessageFormatter::FormatWithNumberedArgs(
                l10n_util::GetStringUTF16(
                    IDS_SETTINGS_UPDATE_REQUIRED_EOL_BANNER_DAYS),
                days_remaining,
                base::UTF8ToUTF16(connector->GetEnterpriseDomainManager()),
                ui::GetChromeOSDeviceName(),
                base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
      }
    }
  }
  html_source->AddString("updateRequiredEolBannerText", eol_return_banner_text);
}

}  // namespace

MainSection::MainSection(Profile* profile,
                         SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {}

MainSection::~MainSection() = default;

void MainSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"add", IDS_ADD},
      {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
      {"back", IDS_ACCNAME_BACK},
      {"basicPageTitle", IDS_SETTINGS_BASIC},
      {"cancel", IDS_CANCEL},
      {"clear", IDS_SETTINGS_CLEAR},
      {"close", IDS_CLOSE},
      {"confirm", IDS_CONFIRM},
      {"continue", IDS_SETTINGS_CONTINUE},
      {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
      {"custom", IDS_SETTINGS_CUSTOM},
      {"delete", IDS_SETTINGS_DELETE},
      {"deviceOff", IDS_SETTINGS_DEVICE_OFF},
      {"deviceOn", IDS_SETTINGS_DEVICE_ON},
      {"disable", IDS_DISABLE},
      {"done", IDS_DONE},
      {"edit", IDS_SETTINGS_EDIT},
      {"extensionsLinkTooltip", IDS_SETTINGS_MENU_EXTENSIONS_LINK_TOOLTIP},
      {"learnMore", IDS_LEARN_MORE},
      {"menu", IDS_MENU},
      {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"ok", IDS_OK},
      {"restart", IDS_SETTINGS_RESTART},
      {"save", IDS_SAVE},
      {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
      {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"settingsAltPageTitle", IDS_SETTINGS_ALT_PAGE_TITLE},
      {"subpageArrowRoleDescription", IDS_SETTINGS_SUBPAGE_BUTTON},
      {"subpageBackButtonAriaLabel",
       IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_LABEL},
      {"subpageBackButtonAriaRoleDescription",
       IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
      {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
      {"notValidWebAddressForContentType",
       IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE},

      // Common font related strings shown in a11y and appearance sections.
      {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
      {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
      {"small", IDS_SETTINGS_SMALL_FONT},
      {"medium", IDS_SETTINGS_MEDIUM_FONT},
      {"large", IDS_SETTINGS_LARGE_FONT},
      {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // This handler is for chrome://os-settings.
  html_source->AddBoolean("isOSSettings", true);

  html_source->AddBoolean("isGuest", features::IsGuestModeActive());
  html_source->AddBoolean(
      "isKioskModeActive",
      user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
  html_source->AddBoolean("isSupervised", profile()->IsSupervised());

  html_source->AddBoolean("isDeepLinkingEnabled",
                          chromeos::features::IsDeepLinkingEnabled());

  // Add the System Web App resources for Settings.
  html_source->AddResourcePath("icon-192.png", IDR_SETTINGS_LOGO_192);

  AddSearchInSettingsStrings(html_source);
  AddChromeOSUserStrings(html_source);
  AddUpdateRequiredEolStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);
}

void MainSection::AddHandlers(content::WebUI* web_ui) {
  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  web_ui->AddMessageHandler(
      std::make_unique<::settings::BrowserLifetimeHandler>());

  web_ui->AddMessageHandler(CreatePluralStringHandler());
}

int MainSection::GetSectionNameMessageId() const {
  NOTIMPLEMENTED();
  return 0;
}

mojom::Section MainSection::GetSection() const {
  NOTIMPLEMENTED();
  return mojom::Section::kMinValue;
}

mojom::SearchResultIcon MainSection::GetSectionIcon() const {
  NOTIMPLEMENTED();
  return mojom::SearchResultIcon::kMinValue;
}

std::string MainSection::GetSectionPath() const {
  NOTIMPLEMENTED();
  return std::string();
}

bool MainSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  NOTIMPLEMENTED();
  return false;
}

void MainSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // MainSection is a container for common resources/functionality shared
  // between sections and does not have its own subpages/settings.
  NOTIMPLEMENTED();
}

void MainSection::AddChromeOSUserStrings(
    content::WebUIDataSource* html_source) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile());
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();

  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddBoolean("isActiveDirectoryUser",
                          user && user->IsActiveDirectoryUser());
  html_source->AddBoolean(
      "isSecondaryUser",
      user && user->GetAccountId() != primary_user->GetAccountId());
  html_source->AddString(
      "secondaryUserBannerText",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SECONDARY_USER_BANNER,
                                 base::ASCIIToUTF16(primary_user_email)));
}

std::unique_ptr<PluralStringHandler> MainSection::CreatePluralStringHandler() {
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
    plural_string_handler->AddLocalizedString("profileLabel",
                                              IDS_OS_SETTINGS_PROFILE_LABEL_V2);
  } else {
    plural_string_handler->AddLocalizedString("profileLabel",
                                              IDS_OS_SETTINGS_PROFILE_LABEL);
  }
  plural_string_handler->AddLocalizedString(
      "nearbyShareContactVisibilityNumUnreachable",
      IDS_NEARBY_CONTACT_VISIBILITY_NUM_UNREACHABLE);

  plural_string_handler->AddLocalizedString(
      "lockScreenNumberFingerprints",
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NUM_FINGERPRINTS);
  return plural_string_handler;
}

}  // namespace settings
}  // namespace chromeos
