#pragma once

#include "common.hpp"
#include "storage/buffer_pool/buffer_partitions.hpp"
#include "storage/buffer_pool/clock.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "storage/mvcc/page_version_manager.hpp"
#include "storage/page.hpp"

namespace db7::storage {

struct PinRangeResult {
  u32 count;       // pages pinned and written to out
  bool io_started; // a read was enqueued for out[0 .. misses)
};

class BufferPool {
private:
  DiskScheduler *disk_mng_;
  Page *pages_;
  u32 pool_size_;
  ClockSweep replacement_;
  BufferPartitions partitions_;
  PageVersionManager *version_table_;

  static constexpr PageIdentifier INVALID_PAGE_IDENTFIER = 0;

  Page *GetVictim(PageIdentifier id, u32 &victim_frame_idx, PageIdentifier &victim_page_id,
                  AccessStrategy &strategy, bool isIO = true);
  void UndoState(Page *victim_page, PageIdentifier victim_page_id);
  bool PageVisit(Page *page, PageIdentifier id);

  enum class PinKind { Hit, Foreign, Miss };
  Page *PinForBlock(PageIdentifier id, AccessStrategy &strategy, PinKind &kind);

public:
  BufferPool(DiskScheduler *disk_mng, PageVersionManager *version_table,
             size_t page_num = BUFFER_POOL_PAGE_NUM);
  ~BufferPool();

  Page *Pin(PageIdentifier id);
  Page *Pin(PageIdentifier id, AccessStrategy &strategy);
  PinRangeResult PinRange(table_id tbl, u32 first_pid, u32 n, Page **out);
  PinRangeResult PinRange(table_id tbl, u32 first_pid, u32 n, Page **out, AccessStrategy &strategy);
  void Unpin(Page *page, bool dirty = false);
  Page *Reserve(table_id tbl_id, AccessStrategy &strategy);

  // TODO remove
  Page *GetPagesDebug() const { return pages_; };
};
} // namespace db7::storage