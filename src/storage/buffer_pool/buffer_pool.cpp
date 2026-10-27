#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/hash_util.hpp"
#include "storage/fsm/fsm_index.hpp"

#include <chrono>
#include <thread>

namespace db7::storage {
BufferPool::BufferPool(DiskScheduler *disk_mng, PageVersionManager *version_table, size_t page_num)
    : disk_mng_(disk_mng), pool_size_(page_num), version_table_(version_table) {
  pages_ = new Page[page_num];
  auto block_size = static_cast<size_t>(page_num) * DB7_PAGE_SIZE;
  auto data = static_cast<u8 *>(std::aligned_alloc(4096, block_size));
  DB7_ASSERT(data != nullptr, "Failed to allocate");
  PageIdentifier id(0);
  for (u32 i = 0; i < page_num; i++) {
    pages_[i].WLock();
    pages_[i].SetId(id);
    pages_[i].WUnlock();
    pages_[i].SetData(data + (i * DB7_PAGE_SIZE));
  }

  size_t per_partition = page_num / BUFFER_POOL_PARTITION_NUM;
  for (u32 i = 0; i < BUFFER_POOL_PARTITION_NUM; i++) { partitions_.Reserve(per_partition, i); }

  FreeSpaceManagerIndex::Reset();
  disk_mng_->io_->RegisterPool(pages_[0].GetData(), block_size);
}

BufferPool::~BufferPool() {
  std::free(pages_[0].GetData());
  delete[] pages_;
}

Page *BufferPool::GetVictim(PageIdentifier id, u32 &victim_frame_idx,
                            PageIdentifier &victim_page_id, bool isIO) {
  u32 max_iters = BUFFER_POOL_PAGE_NUM * 2;

  for (u32 iters = 1; true; iters++) {
    u32 head = (sweep_head_++) % BUFFER_POOL_PAGE_NUM;
    Page *page = &pages_[head];
    if (page->TryWLock()) {
      if (page->IsEvictable()) {
        victim_page_id = page->GetId();
        page->SetId(id);
        page->Pin();
        if (isIO) { page->SetIOInProgress(); }
        page->WUnlock();
        victim_frame_idx = head;
        return page;
      }
      page->WUnlock();
    }

    if (iters % max_iters == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  }

  DB7_UNREACHABLE();
}

void BufferPool::UndoState(Page *victim_page, PageIdentifier victim_page_id) {
  victim_page->WLock();
  victim_page->Unpin();
  victim_page->SetId(victim_page_id);
  victim_page->ClearIOInProgress();
  victim_page->WUnlock();
}

bool BufferPool::PageVisit(Page *page,
                           PageIdentifier id) { // TODO  might be able to use optimistic here but i
                                                // think this is just a spinlock anyway
  // page->RLock();
  // if (page->GetId() == id)
  // { // PageVisit
  //     page->Pin();
  //     page->RUnlock();
  //     return true;
  // }
  // page->RUnlock();

  page->Pin(); // optimistic pin
  if (page->GetId() == id) { return true; }
  page->Unpin();

  return false;
}

u32 BufferPool::GetPartitionIdx(PageIdentifier id) {
  return HashUtil::murmurhash64(id.packed) % BUFFER_POOL_PARTITION_NUM;
}

Page *BufferPool::PinForBlock(PageIdentifier id, PinKind &kind) {
  DB7_ASSERT(id.pid != 0, "invalid pid");
  DB7_ASSERT(id.pid != std::numeric_limits<page_id>::max(), "invalid pid");

  u32 partIdx = GetPartitionIdx(id);

  while (true) {
    u32 frame_idx = partitions_.Get(id, partIdx);
    if (frame_idx != UINT32_MAX) {
      Page *page = &pages_[frame_idx];
      if (PageVisit(page, id)) {
        kind = page->IsIOInProgress() ? PinKind::Foreign : PinKind::Hit;
        return page;
      }
    }

    u32 victim_frame_idx;
    PageIdentifier victim_page_id;
    Page *page = GetVictim(id, victim_frame_idx, victim_page_id); // pinned + IOInProgress

    u32 new_frame_idx;
    if (partitions_.Put(id, victim_frame_idx, new_frame_idx, partIdx)) {
      partitions_.Delete(victim_page_id, victim_frame_idx, GetPartitionIdx(victim_page_id));
      page->SetVersions(version_table_->Get(id));
      kind = PinKind::Miss; // caller must submit the read
      return page;
    }

    UndoState(page, victim_page_id);
    page = &pages_[new_frame_idx];
    if (PageVisit(page, id)) {
      kind = page->IsIOInProgress() ? PinKind::Foreign : PinKind::Hit;
      return page;
    }
  }
}

Page *BufferPool::Pin(PageIdentifier id) { // unchanged behaviour for single-page callers
  PinKind kind;
  Page *page = PinForBlock(id, kind);
  if (kind == PinKind::Miss) disk_mng_->Enqueue(IoTask(IoTask::READ, IoPriority::HIGH, page, id));
  return page;
}

// Pins up to n consecutive pages starting at first_pid into out[].
// Stops after the first hit or foreign page, which is included in out[].
// out[0 .. io_count) are misses, submitted together as one vectored read.
// Returns the number of pages pinned.
PinRangeResult BufferPool::PinRange(table_id tbl, u32 first_pid, u32 n, Page **out) {
  u32 count = 0;
  u32 misses = 0;
  while (count < n) {
    PinKind kind;
    out[count] = PinForBlock(PageIdentifier(tbl, first_pid + count), kind);
    count++;
    if (kind != PinKind::Miss)
      break; // cached or being loaded: ends the run, but it's pinned, so return it
    misses++;
  }

  if (misses > 0)
    disk_mng_->Enqueue(
        IoTask(IoTask::READV, IoPriority::HIGH, out, misses, PageIdentifier(tbl, first_pid)));
  return {count, misses > 0};
}

void BufferPool::Unpin(Page *page, bool dirty) {
  (void)dirty;
  // page->WLock();
  page->Unpin();
  // page->WUnlock();
}

Page *BufferPool::Reserve(table_id tbl_id) {
  u32 pid = FreeSpaceManagerIndex::Get(tbl_id);
  // TODO just temporary guard
  DB7_ASSERT(pid < BUFFER_POOL_PAGE_NUM, "no more pages");

  auto id = PageIdentifier(tbl_id, pid);
  u32 partIdx = GetPartitionIdx(id);

  u32 victim_frame_idx;
  PageIdentifier victim_page_id(0);
  auto page = GetVictim(id, victim_frame_idx, victim_page_id, false);

  u32 new_frame_idx;
  if (partitions_.Put(id, victim_frame_idx, new_frame_idx, partIdx)) {
    partIdx = GetPartitionIdx(victim_page_id);
    partitions_.Delete(victim_page_id, victim_frame_idx, partIdx);
    std::memset(page->GetData(), 0, DB7_PAGE_SIZE); // TODO i dont needd this
    return page;
  }

  DB7_UNREACHABLE();
}
} // namespace db7::storage