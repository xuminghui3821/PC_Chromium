// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_

#include "extensions/browser/api/file_system/file_system_delegate.h"

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace file_system_api {

// Dispatches an event about a mounted or unmounted volume in the system to
// each extension which can request it.
void DispatchVolumeListChangeEvent(content::BrowserContext* browser_context);

}  // namespace file_system_api
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ChromeFileSystemDelegate : public FileSystemDelegate {
 public:
  ChromeFileSystemDelegate();
  ~ChromeFileSystemDelegate() override;

  // FileSystemDelegate:
  base::FilePath GetDefaultDirectory() override;
  bool ShowSelectFileDialog(
      scoped_refptr<ExtensionFunction> extension_function,
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path,
      const ui::SelectFileDialog::FileTypeInfo* file_types,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback) override;
  void ConfirmSensitiveDirectoryAccess(bool has_write_permission,
                                       const std::u16string& app_name,
                                       content::WebContents* web_contents,
                                       base::OnceClosure on_accept,
                                       base::OnceClosure on_cancel) override;
  int GetDescriptionIdForAcceptType(const std::string& accept_type) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  FileSystemDelegate::GrantVolumesMode GetGrantVolumesMode(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension) override;
  void RequestFileSystem(content::BrowserContext* browser_context,
                         scoped_refptr<ExtensionFunction> requester,
                         const Extension& extension,
                         std::string volume_id,
                         bool writable,
                         FileSystemCallback success_callback,
                         ErrorCallback error_callback) override;
  void GetVolumeList(content::BrowserContext* browser_context,
                     const Extension& extension,
                     VolumeListCallback success_callback,
                     ErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeFileSystemDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_
