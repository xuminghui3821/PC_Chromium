// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "base/stl_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view_impl.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/web_contents.h"

using autofill::mojom::FocusedFieldType;
using password_manager::PasswordManagerClient;

// No-op constructor for tests.
AllPasswordsBottomSheetController::AllPasswordsBottomSheetController(
    base::PassKey<class AllPasswordsBottomSheetControllerTest>,
    std::unique_ptr<AllPasswordsBottomSheetView> view,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    password_manager::PasswordStore* store,
    base::OnceCallback<void()> dismissal_callback,
    FocusedFieldType focused_field_type,
    PasswordManagerClient* client)
    : view_(std::move(view)),
      store_(store),
      dismissal_callback_(std::move(dismissal_callback)),
      driver_(std::move(driver)),
      focused_field_type_(focused_field_type),
      client_(client) {}

AllPasswordsBottomSheetController::AllPasswordsBottomSheetController(
    content::WebContents* web_contents,
    password_manager::PasswordStore* store,
    base::OnceCallback<void()> dismissal_callback,
    FocusedFieldType focused_field_type)
    : view_(std::make_unique<AllPasswordsBottomSheetViewImpl>(this)),
      web_contents_(web_contents),
      store_(store),
      dismissal_callback_(std::move(dismissal_callback)),
      focused_field_type_(focused_field_type) {
  DCHECK(web_contents_);
  DCHECK(store_);
  DCHECK(dismissal_callback_);
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetFocusedFrame());
  driver_ = driver->AsWeakPtr();
  client_ = ChromePasswordManagerClient::FromWebContents(web_contents_);
}

AllPasswordsBottomSheetController::~AllPasswordsBottomSheetController() =
    default;

void AllPasswordsBottomSheetController::Show() {
  store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

void AllPasswordsBottomSheetController::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  base::EraseIf(results,
                [](const auto& form_ptr) { return form_ptr->blocked_by_user; });
  view_->Show(std::move(results), focused_field_type_);
}

gfx::NativeView AllPasswordsBottomSheetController::GetNativeView() {
  return web_contents_->GetNativeView();
}

void AllPasswordsBottomSheetController::OnCredentialSelected(
    const std::u16string username,
    const std::u16string password) {
  const bool is_password_field =
      focused_field_type_ == FocusedFieldType::kFillablePasswordField;
  DCHECK(driver_);
  if (is_password_field) {
    driver_->FillIntoFocusedField(is_password_field, password);

    // `client_` is guaranteed to be valid here.
    // Both the `client_` and `PasswordAccessoryController` are attached to
    // WebContents. And AllPasswordBottomSheetController is owned by
    // PasswordAccessoryController.
    DCHECK(client_);
    client_->OnPasswordSelected(password);
  } else {
    driver_->FillIntoFocusedField(is_password_field, username);
  }

  // Consumes the dismissal callback to destroy the native controller and java
  // controller after the user selects a credential.
  OnDismiss();
}

void AllPasswordsBottomSheetController::OnDismiss() {
  std::move(dismissal_callback_).Run();
}

const GURL& AllPasswordsBottomSheetController::GetFrameUrl() {
  return driver_->GetLastCommittedURL();
}
