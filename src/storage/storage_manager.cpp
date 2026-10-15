#include "storage/storage_manager.hpp"

#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "storage/mvcc/page_version_manager.hpp"

namespace db7::storage {
StorageManager::StorageManager(std::string base_file_folder)
    : disk_manager_(std::make_unique<DiskManagerAsync>(base_file_folder.c_str())),
      disk_scheduler_(std::make_unique<DiskScheduler>(disk_manager_.get())),
      version_manager_(std::make_unique<PageVersionManager>()),
      buffer_pool_(std::make_unique<BufferPool>(disk_scheduler_.get(), version_manager_.get())) {
  disk_scheduler_->Start();
}
StorageManager::~StorageManager() = default;

storage::BufferPool &StorageManager::GetBufferPool() { return *buffer_pool_; }
storage::DiskManagerAsync &StorageManager::GetDiskManager() { return *disk_manager_; };
storage::DiskScheduler &StorageManager::GetDiskScheduler() { return *disk_scheduler_; };
storage::PageVersionManager &StorageManager::GetVersionManager() { return *version_manager_; };
} // namespace db7::storage