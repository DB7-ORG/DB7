#pragma once

#include "common.hpp"
#include "shared/data_structures/ring_buffer.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/page.hpp"

namespace db7::storage {

enum StreamType { SCAN, SCAN_UNORDERED };

class BufferAccessorBase {
private:
  StreamType type_;
  table_id table_id_;

public:
  BufferAccessorBase(StreamType type, table_id table_id) : type_(type), table_id_(table_id) {}
  StreamType GetType() const { return type_; }
  table_id GetTableId() const { return table_id_; }

  virtual ~BufferAccessorBase() = default;
  virtual Page *Next() = 0;
};

/* Not thread safe */
class ScanBufferAccessor : public BufferAccessorBase {
private:
  u32 nblocks_;           // pages in the table
  u32 next_block_ = 0;    // next block to pin (Postgres: rs_prefetch_block)
  u32 distance_ = 1;      // current look-ahead (Postgres: readahead_distance)
  u32 decay_holdoff_ = 0; // don't shrink right after we had to wait
  shared::Ringbuffer<Page *> ring_;
  BufferPool *pool_;

  void LookAhead() {
    while (ring_.Size() < distance_ && next_block_ < nblocks_) {
      auto *page = pool_->Pin({GetTableId(), next_block_++});
      if (!page->IsIOInProgress()) {
        if (decay_holdoff_ > 0)
          decay_holdoff_--;
        else if (distance_ > 1)
          distance_--; // cached: look-ahead isn't buying anything
      }
      ring_.Append(page);
    }
  }

public:
  ScanBufferAccessor(BufferPool *pool, table_id tbl, u32 nblocks, u32 max_distance)
      : BufferAccessorBase(StreamType::SCAN, tbl), nblocks_(nblocks), pool_(pool) {}

  ~ScanBufferAccessor() override { // scan ended early (LIMIT, error)
    for (auto r = ring_.Read(); r.success; r = ring_.Read()) pool_->Unpin(r.value);
  }

  // Returns a pinned page; the caller must Unpin it. Returns nullptr at the end.
  Page *Next() override {
    if (ring_.Size() == 0) {
      LookAhead();
      if (ring_.Size() == 0) return nullptr;
    }

    Page *page = ring_.ReadUnsafe();

    if (page->IsIOInProgress()) {
      distance_ = std::min(distance_ * 2, ring_.MaxSize());
      decay_holdoff_ = ring_.MaxSize();
      page->WaitIO();
    }

    LookAhead(); // refill the slot we just freed
    return page;
  }
};

/* Not thread safe */
class ScanUnorderedBufferAccessor : public BufferAccessorBase {
private:
  u32 nblocks_;           // pages in the table
  u32 next_block_ = 0;    // next block to pin (Postgres: rs_prefetch_block)
  u32 distance_ = 1;      // current look-ahead (Postgres: readahead_distance)
  u32 decay_holdoff_ = 0; // don't shrink right after we had to wait
  u32 count_ = 0;
  shared::Ringbuffer<Page *> wait_ring_;
  shared::Ringbuffer<Page *> ready_ring_;
  BufferPool *pool_;

  void LookAhead() {
    while (count_ < distance_ && next_block_ < nblocks_) {
      auto *page = pool_->Pin({GetTableId(), next_block_++});
      count_++;
      if (!page->IsIOInProgress()) {
        if (decay_holdoff_ > 0)
          decay_holdoff_--;
        else if (distance_ > 1)
          distance_--; // cached: look-ahead isn't buying anything

        ready_ring_.Append(page);
      } else {
        wait_ring_.Append(page);
      }
    }
  }

  u32 Count() const { return ready_ring_.Size() + wait_ring_.Size(); }

public:
  ScanUnorderedBufferAccessor(BufferPool *pool, table_id tbl, u32 nblocks, u32 max_distance)
      : BufferAccessorBase(StreamType::SCAN_UNORDERED, tbl), nblocks_(nblocks), pool_(pool) {}

  ~ScanUnorderedBufferAccessor() override { // scan ended early (LIMIT, error)
    for (auto r = wait_ring_.Read(); r.success; r = wait_ring_.Read()) pool_->Unpin(r.value);
    for (auto r = ready_ring_.Read(); r.success; r = ready_ring_.Read()) pool_->Unpin(r.value);
  }

  // Returns a pinned page; the caller must Unpin it. Returns nullptr at the end.
  Page *Next() override {
    if (count_ == 0) {
      LookAhead();
      if (count_ == 0) return nullptr;
    }

    auto result = ready_ring_.Read();
    count_--;
    if (result.success) {
      Page *page = result.value;
      LookAhead();
      return page;
    }

    Page *page = wait_ring_.ReadUnsafe();
    if (page->IsIOInProgress()) {
      distance_ = std::min(distance_ * 2, wait_ring_.MaxSize());
      decay_holdoff_ = wait_ring_.MaxSize();
      page->WaitIO();
    }

    LookAhead(); // refill the slot we just freed
    return page;
  }
};
} // namespace db7::storage