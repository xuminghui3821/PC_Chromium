// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CALCULATOR_SIZE_CALCULATOR_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CALCULATOR_SIZE_CALCULATOR_H_

#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/arc/mojom/storage_manager.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/user_manager/user.h"

class Profile;

namespace chromeos {
namespace settings {
namespace calculator {

// Base class for the calculation of a specific storage item. Instances of this
// class rely on their observers calling StartCalculation, and are designed to
// notify observers about the calculated sizes.
class SizeCalculator {
 public:
  // Enumeration listing the items displayed on the storage page.
  enum class CalculationType {
    kInUse = 0,
    kMyFiles,
    kBrowsingData,
    kAppsExtensions,
    kCrostini,
    kOtherUsers,
    kLast = kOtherUsers,
    kSystem,
  };

  // Implement this interface to be notified about item size callbacks.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSizeCalculated(
        const CalculationType& item_id,
        int64_t total_bytes,
        const base::Optional<int64_t>& available_bytes) = 0;
  };

  // Total number of storage items.
  static constexpr int kCalculationTypeCount =
      static_cast<int>(CalculationType::kLast) + 1;

  explicit SizeCalculator(const CalculationType& calculation_type);
  virtual ~SizeCalculator();

  // Starts the size calculation of a given storage item.
  void StartCalculation();

  // Adds an observer.
  virtual void AddObserver(Observer* observer);

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer);

 protected:
  // Performs the size calculation.
  virtual void PerformCalculation() = 0;

  // Notify the StorageHandler about the calculated storage item size
  void NotifySizeCalculated(
      int64_t total_bytes,
      const base::Optional<int64_t>& available_bytes = base::nullopt);

  // Item id.
  const CalculationType calculation_type_;

  // Flag indicating that fetch operations for storage size are ongoing.
  bool calculating_ = false;
  // Observers being notified about storage items size changes.
  base::ObserverList<SizeCalculator::Observer> observers_;
};

// Class handling the interactions with the filesystem to get storage
// statistics, using OnSizeStatCalculated to notify observers.
class SizeStatCalculator : public SizeCalculator {
 public:
  explicit SizeStatCalculator(Profile* profile);
  ~SizeStatCalculator() override;

  SizeStatCalculator(const SizeStatCalculator&) = delete;
  SizeStatCalculator& operator=(const SizeStatCalculator&) = delete;

 private:
  friend class SizeStatTestAPI;

  void PerformCalculation() override;

  // Updates disk space information.
  void OnGetSizeStat(int64_t* total_bytes, int64_t* available_bytes);

  Profile* profile_;
  base::WeakPtrFactory<SizeStatCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the size of the user's personal files: My
// files + Android Play files.
class MyFilesSizeCalculator : public SizeCalculator {
 public:
  explicit MyFilesSizeCalculator(Profile* profile);
  ~MyFilesSizeCalculator() override;

  MyFilesSizeCalculator(const MyFilesSizeCalculator&) = delete;
  MyFilesSizeCalculator& operator=(const MyFilesSizeCalculator&) = delete;

 private:
  friend class MyFilesSizeTestAPI;

  void PerformCalculation() override;

  // Computes the size of My Files and Play files.
  int64_t ComputeLocalFilesSize(const base::FilePath& my_files_path,
                                const base::FilePath& android_files_path);

  // Updates the size of My Files and Play files.
  void OnGetMyFilesSize(int64_t total_bytes);

  Profile* profile_;
  base::WeakPtrFactory<MyFilesSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of browsing data and cache.
class BrowsingDataSizeCalculator : public SizeCalculator {
 public:
  explicit BrowsingDataSizeCalculator(Profile* profile);
  ~BrowsingDataSizeCalculator() override;

  BrowsingDataSizeCalculator(const BrowsingDataSizeCalculator&) = delete;
  BrowsingDataSizeCalculator& operator=(const BrowsingDataSizeCalculator&) =
      delete;

 private:
  friend class BrowsingDataSizeTestAPI;

  void PerformCalculation() override;

  // Callback to receive the cache size.
  void OnGetCacheSize(bool is_upper_limit, int64_t size);

  // Callback to update the size of browsing data.
  void OnGetBrowsingDataSize(bool is_site_data, int64_t size);

  // Total size of cache data in browsing data.
  int64_t browser_cache_size_ = -1;

  // True if we have already received the size of http cache.
  bool has_browser_cache_size_ = false;

  // Total size of site data in browsing data.
  int64_t browser_site_data_size_ = -1;

  // True if we have already received the size of http cache.
  bool has_browser_site_data_size_ = false;

  // Helper to compute the total size of all types of site date.
  std::unique_ptr<SiteDataSizeCollector> site_data_size_collector_;

  Profile* profile_;
  base::WeakPtrFactory<BrowsingDataSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the size of the user's apps and extensions.
class AppsSizeCalculator
    : public SizeCalculator,
      public arc::ConnectionObserver<arc::mojom::StorageManagerInstance> {
 public:
  explicit AppsSizeCalculator(Profile* profile);
  ~AppsSizeCalculator() override;

  AppsSizeCalculator(const AppsSizeCalculator&) = delete;
  AppsSizeCalculator& operator=(const AppsSizeCalculator&) = delete;

  // arc::ConnectionObserver<arc::mojom::StorageManagerInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // Adds an observer. When the first observer is added, start observing the arc
  // mojo connection UpdateAndroidAppsSize relies on.
  void AddObserver(Observer* observer) override;

  // Removes an observer. When the last observer is removed, stop observing the
  // arc mojo connection.
  void RemoveObserver(Observer* observer) override;

 private:
  friend class AppsSizeTestAPI;

  void PerformCalculation() override;

  // Requests updating the size of web store apps and extensions.
  void UpdateAppsSize();

  // Callback to update web store apps and extensions size.
  void OnGetAppsSize(int64_t total_bytes);

  // Requests updating the size of android apps.
  void UpdateAndroidAppsSize();

  // Callback to update Android apps and cache.
  void OnGetAndroidAppsSize(bool succeeded,
                            arc::mojom::ApplicationsSizePtr size);

  // Updates apps and extensions size.
  void UpdateAppsAndExtensionsSize();

  // Total size of apps and extensions
  int64_t apps_extensions_size_ = 0;

  // True if we have already received the size of apps and extensions.
  bool has_apps_extensions_size_ = false;

  // Total size of android apps
  int64_t android_apps_size_ = 0;

  // True if we have already received the size of Android apps.
  bool has_android_apps_size_ = false;

  // A flag for keeping track of the mojo connection status to the ARC
  // container.
  bool is_android_running_ = false;

  Profile* profile_;
  base::WeakPtrFactory<AppsSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of crostini VM size.
class CrostiniSizeCalculator : public SizeCalculator {
 public:
  explicit CrostiniSizeCalculator(Profile* profile);
  ~CrostiniSizeCalculator() override;

  CrostiniSizeCalculator(const CrostiniSizeCalculator&) = delete;
  CrostiniSizeCalculator& operator=(const CrostiniSizeCalculator&) = delete;

 private:
  friend class CrostiniSizeTestAPI;

  void PerformCalculation() override;

  // Callback to update the size of Crostini VMs.
  void OnGetCrostiniSize(crostini::CrostiniResult result, int64_t size);

  Profile* profile_;
  base::WeakPtrFactory<CrostiniSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of other users' cryptohomes.
class OtherUsersSizeCalculator : public SizeCalculator {
 public:
  OtherUsersSizeCalculator();
  ~OtherUsersSizeCalculator() override;

  OtherUsersSizeCalculator(const OtherUsersSizeCalculator&) = delete;
  OtherUsersSizeCalculator& operator=(const OtherUsersSizeCalculator&) = delete;

 private:
  friend class OtherUsersSizeTestAPI;

  void PerformCalculation() override;

  // Callback to update the sizes of the other users.
  void OnGetOtherUserSize(
      base::Optional<::user_data_auth::GetAccountDiskUsageReply> reply);

  // The list of other users whose directory sizes will be accumulated as the
  // size of "Other users".
  user_manager::UserList other_users_;

  // Fetched sizes of user directories.
  std::vector<int64_t> user_sizes_;

  base::WeakPtrFactory<OtherUsersSizeCalculator> weak_ptr_factory_{this};
};

}  // namespace calculator
}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CALCULATOR_SIZE_CALCULATOR_H_
