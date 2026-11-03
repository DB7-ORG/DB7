#pragma once

#include "common.hpp"
#include "shared/data_structures/partitioned_hashmap.hpp"
#include "storage/buffer_pool/clock.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "storage/mvcc/page_version_manager.hpp"
#include "storage/page.hpp"

namespace db7::storage {

class BgWriter;

struct PinRangeResult {
  u32 count;       // pages pinned and written to out
  bool io_started; // a read was enqueued for out[0 .. misses)
};

struct PinOptions {
  PageKind pkind;
  AccessStrategy &strategy;
  IoPriority priority = IoPriority::HIGH;

  PinOptions(PageKind pk, AccessStrategy &strat) : pkind(pk), strategy(strat) {}
};

struct PageRange {
  PageIdentifier first;
  u32 size;
};

class BufferPool {
private:
  friend class BgWriter;
  DiskScheduler *disk_mng_;
  Page *pages_;
  frame_idx_t pool_size_;
  PageVersionManager *version_table_;
  shared::PartitionedHashMap<PageIdentifier, frame_idx_t, BUFFER_POOL_PARTITION_NUM> partitions_;
  // replacement should be stored at the end for cache line alignment
  alignas(CACHE_LINE_SIZE) ClockSweep replacement_;

  static constexpr PageIdentifier INVALID_PAGE_IDENTFIER = 0;

  std::span<Page> GetPages() const { return {pages_, pool_size_}; }
  void UndoState(VictimPage victim);
  bool PageVisit(Page *page, PageIdentifier id);

  enum class PinKind { Hit, Foreign, Miss };
  struct PinResult {
    Page *page;
    PinKind kind;
  };
  struct DeferRead {
    bool operator()(Page *, PageIdentifier) const { return true; }
  };
  template <typename ReadFn = DeferRead>
  PinResult PinForBlock(PageIdentifier id, PinOptions &opts, ReadFn read_fn = {});

  bool Flush(Page *page);

public:
  BufferPool(DiskScheduler *disk_mng, PageVersionManager *version_table,
             frame_idx_t page_num = BUFFER_POOL_PAGE_NUM);
  ~BufferPool();

  Page *Pin(PageIdentifier id, PageKind pkind);
  Page *Pin(PageIdentifier id, PinOptions &opts);
  PinRangeResult PinRange(PageRange range, Page **out, PageKind pkind);
  PinRangeResult PinRange(PageRange range, Page **out, PinOptions &opts);
  void Unpin(Page *page, bool dirty = false);

  /* Only used for indexes for now should be more flexible in future */
  Page *Reserve(table_id tbl_id, AccessStrategy &strategy);

  /**
   * Finds a victim page to evict considering an access strategy policy.
   *
   * @param pages_ main buffer pool buffer that stores pages
   * @param id     id of a new page that needs to take its place
   * @param isIO   should it set IO_IN_PROGRESS flag
   */
  VictimPage GetVictim(PageIdentifier id, AccessStrategy &strategy, bool isIO);

  // TODO remove
  Page *GetPagesDebug() const { return pages_; };
};
} // namespace db7::storage