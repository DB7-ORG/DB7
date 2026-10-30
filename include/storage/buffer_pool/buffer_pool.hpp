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
  frame_idx_t pool_size_;
  ClockSweep replacement_;
  PartitionedHashMap<PageIdentifier, frame_idx_t, BUFFER_POOL_PARTITION_NUM> partitions_;
  PageVersionManager *version_table_;

  static constexpr PageIdentifier INVALID_PAGE_IDENTFIER = 0;

  std::span<Page> GetPages() { return {pages_, pool_size_}; }
  void UndoState(VictimPage victim);
  bool PageVisit(Page *page, PageIdentifier id);

  enum class PinKind { Hit, Foreign, Miss };
  Page *PinForBlock(PageIdentifier id, AccessStrategy &strategy, bool isHeapPage, PinKind &kind);

public:
  BufferPool(DiskScheduler *disk_mng, PageVersionManager *version_table,
             frame_idx_t page_num = BUFFER_POOL_PAGE_NUM);
  ~BufferPool();

  Page *Pin(PageIdentifier id, bool isHeapPage);
  Page *Pin(PageIdentifier id, AccessStrategy &strategy, bool isHeapPage);
  PinRangeResult PinRange(PageIdentifier first_id, u32 n, Page **out, bool isHeapPage);
  PinRangeResult PinRange(PageIdentifier first_id, u32 n, Page **out, AccessStrategy &strategy,
                          bool isHeapPage);
  void Unpin(Page *page, bool dirty = false);

  /* Only used for indexes for now should be more flexible in future */
  Page *Reserve(table_id tbl_id, AccessStrategy &strategy);

  // TODO remove
  Page *GetPagesDebug() const { return pages_; };
};
} // namespace db7::storage