// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class ListValue;
}

namespace extensions {
namespace api {
namespace file_manager_private {
struct FileTransferStatus;
struct FileWatchEvent;
}  // namespace file_manager_private
}  // namespace api
}  // namespace extensions

namespace file_manager {

// Files app's event router handling DriveFS-related events.
class DriveFsEventRouter : public drivefs::DriveFsHostObserver {
 public:
  DriveFsEventRouter();
  virtual ~DriveFsEventRouter();

  // Triggers an event in the UI to display a confirmation dialog.
  void DisplayConfirmDialog(
      const drivefs::mojom::DialogReason& reason,
      base::OnceCallback<void(drivefs::mojom::DialogResult)> callback);

  // Called from the UI to notify the caller of DisplayConfirmDialog() of the
  // dialog's result.
  void OnDialogResult(drivefs::mojom::DialogResult result);

 private:
  struct SyncingStatusState {
    SyncingStatusState();
    ~SyncingStatusState();

    std::map<int64_t, int64_t> group_id_to_bytes_to_transfer;
    int64_t completed_bytes = 0;
  };

  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  void DispatchOnFileTransfersUpdatedEvent(
      const extensions::api::file_manager_private::FileTransferStatus& status);

  virtual std::set<std::string> GetEventListenerExtensionIds(
      const std::string& event_name) = 0;

  virtual GURL ConvertDrivePathToFileSystemUrl(
      const base::FilePath& file_path,
      const std::string& extension_id) = 0;

  virtual std::string GetDriveFileSystemName() = 0;

  virtual bool IsPathWatched(const base::FilePath& path) = 0;

  void DispatchOnFileTransfersUpdatedEventToExtension(
      const std::string& extension_id,
      const extensions::api::file_manager_private::FileTransferStatus& status);

  void DispatchOnPinTransfersUpdatedEvent(
      const extensions::api::file_manager_private::FileTransferStatus& status);

  void DispatchOnPinTransfersUpdatedEventToExtension(
      const std::string& extension_id,
      const extensions::api::file_manager_private::FileTransferStatus& status);

  void DispatchOnDirectoryChangedEventToExtension(
      const std::string& extension_id,
      const base::FilePath& directory,
      const extensions::api::file_manager_private::FileWatchEvent& event);

  // Helper method for dispatching an event to an extension.
  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> event_args) = 0;

  static extensions::api::file_manager_private::FileTransferStatus
  CreateFileTransferStatus(
      const std::vector<drivefs::mojom::ItemEvent*>& item_events,
      SyncingStatusState* state);

  SyncingStatusState sync_status_state_;
  SyncingStatusState pin_status_state_;
  base::OnceCallback<void(drivefs::mojom::DialogResult)> dialog_callback_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsEventRouter);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
