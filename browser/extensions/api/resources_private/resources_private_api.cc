// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/api/resources_private.h"
#include "chrome/grit/generated_resources.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_PDF)

// To add a new component to this API, simply:
// 1. Add your component to the Component enum in
//      chrome/common/extensions/api/resources_private.idl
// 2. Create an AddStringsForMyComponent(base::DictionaryValue * dict) method.
// 3. Tie in that method to the switch statement in Run()

namespace extensions {

namespace {

void AddStringsForIdentity(base::DictionaryValue* dict) {
  dict->SetString("window-title",
                  l10n_util::GetStringUTF16(IDS_EXTENSION_CONFIRM_PERMISSIONS));
}

#if BUILDFLAG(ENABLE_PDF)
bool IsPdfAnnotationsEnabled(content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* prefs =
      context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
  if (prefs && prefs->IsManagedPreference(prefs::kPdfAnnotationsEnabled) &&
      !prefs->GetBoolean(prefs::kPdfAnnotationsEnabled)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
}
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace

namespace get_strings = api::resources_private::GetStrings;

ResourcesPrivateGetStringsFunction::ResourcesPrivateGetStringsFunction() {}

ResourcesPrivateGetStringsFunction::~ResourcesPrivateGetStringsFunction() {}

ExtensionFunction::ResponseAction ResourcesPrivateGetStringsFunction::Run() {
  std::unique_ptr<get_strings::Params> params(
      get_strings::Params::Create(*args_));
  auto dict = std::make_unique<base::DictionaryValue>();

  api::resources_private::Component component = params->component;

  switch (component) {
    case api::resources_private::COMPONENT_IDENTITY:
      AddStringsForIdentity(dict.get());
      break;
#if BUILDFLAG(ENABLE_PDF)
    case api::resources_private::COMPONENT_PDF: {
      pdf_extension_util::AddStrings(pdf_extension_util::PdfViewerContext::kAll,
                                     dict.get());
      pdf_extension_util::AddAdditionalData(
          IsPdfAnnotationsEnabled(browser_context()), dict.get());
    } break;
#endif  // BUILDFLAG(ENABLE_PDF)
    case api::resources_private::COMPONENT_NONE:
      NOTREACHED();
  }

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
}

}  // namespace extensions
