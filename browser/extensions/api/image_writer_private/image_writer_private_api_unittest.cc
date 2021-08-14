// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {
#if BUILDFLAG(IS_CHROMEOS_ASH)
using api::image_writer_private::RemovableStorageDevice;
class ImageWriterPrivateApiUnittest : public ExtensionApiUnittest {
 public:
  ImageWriterPrivateApiUnittest() {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    auto device_list = base::MakeRefCounted<StorageDeviceList>();
    RemovableStorageDevice expected;
    expected.vendor = "Vendor 1";
    expected.model = "Model 1";
    expected.capacity = image_writer::kTestFileSize;
    expected.removable = true;
    device_list->data.push_back(std::move(expected));
    RemovableStorageProvider::SetDeviceListForTesting(device_list);
  }

  void TearDown() override {
    RemovableStorageProvider::ClearDeviceListForTesting();
    ExtensionApiUnittest::TearDown();
  }
};

TEST_F(ImageWriterPrivateApiUnittest,
       TestStorageDisabledPolicyReturnsEmptyList) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kExternalStorageDisabled, true);
  auto function = base::MakeRefCounted<
      ImageWriterPrivateListRemovableStorageDevicesFunction>();
  std::unique_ptr<base::ListValue> devices =
      RunFunctionAndReturnList(function.get(), "[]");
  ASSERT_TRUE(devices.get() ? devices.get()->empty() : false)
      << "Under policy ListDevices should return an empty list.";
}

TEST_F(ImageWriterPrivateApiUnittest,
       TestExternalStorageReadOnlyPolicyFailsWrite) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kExternalStorageDisabled, false);
  prefs->SetBoolean(prefs::kExternalStorageReadOnly, true);
  auto function =
      base::MakeRefCounted<ImageWriterPrivateWriteFromFileFunction>();
  ASSERT_TRUE(
      base::MatchPattern(RunFunctionAndReturnError(function.get(), "[]"),
                         image_writer::error::kDeviceWriteError));
}

#endif  // if BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions