#pragma once

#include <memory>

namespace db7 {
namespace storage {

class BufferPool;
class DiskManagerAsync;
class DiskScheduler;
class PageVersionManager;

class StorageManager {
private:
  std::unique_ptr<storage::DiskManagerAsync> disk_manager_;
  std::unique_ptr<storage::DiskScheduler> disk_scheduler_;
  std::unique_ptr<storage::PageVersionManager> version_manager_;
  std::unique_ptr<storage::BufferPool> buffer_pool_;

public:
  explicit StorageManager(std::string base_file_folder);
  ~StorageManager();

  StorageManager(const StorageManager &) = delete;
  StorageManager &operator=(const StorageManager &) = delete;
  StorageManager(StorageManager &&) = delete;
  StorageManager &operator=(StorageManager &&) = delete;

  storage::BufferPool &GetBufferPool();
  storage::DiskManagerAsync &GetDiskManager();
  storage::DiskScheduler &GetDiskScheduler();
  storage::PageVersionManager &GetVersionManager();
};
} // namespace storage
} // namespace db7