#pragma once

#include "storage/disk_manager/fd_cache.hpp"
#include "storage/page.hpp"
#include "storage/storage_common.hpp"

#include <functional>
#include <liburing.h>
#include <memory>

#include <linux/futex.h>
#include <sys/syscall.h>

/**
 * this is a hack
 */
#define MAX_PATH_LEN 256u
#define MAX_PATH_LEN_2 128u
#define DIRECT_ALIGN 4096
#define INIT_FREE_PAGES 3
#define IO_MAX_COMBINE 32

namespace db7::storage {
class DiskManagerAsync {
private:
  char base_dir_[MAX_PATH_LEN_2]; /* directory that holds table files */
  std::unique_ptr<FdCache> cache_;

  struct io_uring ring_;
  using CompletionCb = std::function<void(void *user_data, ssize_t result)>;
  CompletionCb completion_cb_;

  static constexpr size_t kRegChunk = 1ull << 30;
  u8 *reg_base_ = nullptr;

  static constexpr u32 kMaxIov = IO_MAX_COMBINE;
  std::unique_ptr<iovec[]> iov_store_; // kMaxIov iovecs per SQE slot
  u32 sqe_shift_ = 0;                  // 1 if the ring uses 128-byte SQEs

  alignas(4) u32 wake_seq_ = 0; // the futex word; replaces efd_ and efd_buf_

  void BuildPath(table_id tid, char *buf, u32 len);
  void LoadExistingTables();
  u32 RegIndex(const void *buf) const;
  iovec *IovFor(io_uring_sqe *sqe);

public:
  explicit DiskManagerAsync(const char *base_dir, u32 queue_depth = IOURING_QUEUE_SIZE);
  ~DiskManagerAsync();
  void SetCompletionCallback(CompletionCb cb) { completion_cb_ = std::move(cb); }

  /**
   * Optimization that registers your upfront buffer pool block of memory for io uring to use.
   * This is an optimization that prevents table lookups and extra pinning that kernel needs
   * to do on every read/write request.
   */
  bool RegisterPool(u8 *base, size_t len);

  bool SubmitRead(table_id pid, void *buf, u32 len, off_t offset, void *user_data);
  bool SubmitReadv(table_id tid, Page *const *pages, u32 nr, off_t offset, void *user_data);
  bool SubmitWrite(table_id pid, const void *buf, u32 len, off_t offset, void *user_data);
  bool SubmitWritev(table_id tid, Page *const *pages, u32 nr, off_t offset, void *user_data);
  int ReapCompletions(u32 max_completions = IOURING_QUEUE_SIZE);
  int Submit();

  bool CreateTable(table_id tbl_id, u32 initial_pages = INIT_FREE_PAGES);
  bool OpenFile(table_id tbl_id);
  bool CreateOpenFile(table_id tbl_id, u32 initial_pages);
  bool DropTable(table_id tbl_id);
  bool ExistsTable(table_id tbl_id);
  bool ExtendFile(table_id tbl_id, u64 pages_num);
  u32 PageCount(table_id tbl_id);
  bool DeleteDir(const char *directory);

  /* Scheduler thread wakeup mechanism */
  void InitWakeup();
  void ArmWakeup();
  void Wake();
  void WaitForCompletion();
};
} // namespace db7::storage