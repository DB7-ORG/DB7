#pragma once

#include "storage/buffer_pool/iostream.hpp"
#include "storage/columns_chunk.hpp"
#include "storage/layouts/pax.hpp"
#include "storage/models/parallel_scan_shared.hpp"

namespace db7::storage {

struct ScanCursor {
  // stores global cursor for scanning a table
  ParallelScanShared &shared;
  // stores local cursor
  ScanChunk cursor;

  explicit ScanCursor(ParallelScanShared &sh) : shared(sh) {}

  page_id operator()() { // what the stream calls as next_fn_()
    if (cursor.start == cursor.end && !shared.Next(cursor)) return INVALID_BLOCK;
    return page_id(cursor.start++);
  }
};

class PageCursor {
private:
  const std::vector<StorageColumnDesc> &columns_;
  std::vector<ColumnVector> vecs_;
  std::vector<PinnedPage> held_; // earlier pages the current chunk's strings point into
  PinnedPage page_;              // page being read right now
  u16 start_ = 0;                // tuple offset in page_
  u16 rows_ = 0;                 // total rows in page_

public:
  explicit PageCursor(const std::vector<StorageColumnDesc> &columns)
      : columns_(columns), vecs_(columns.size()) {}

  /** Takes the pin. Page data is not read until Load(). */
  void Assign(PinnedPage page) {
    if (page_) held_.push_back(std::move(page_));
    page_ = std::move(page);
    start_ = rows_ = 0;
  }

  PinnedPage &Page() { return page_; }

  void Drop() { page_.reset(); }
  bool IsBlocked() const { return page_->IsIOOrWriteInProgress(); }
  byte *Data() const { return page_->GetData(); }

  bool Exhausted() const { return start_ >= rows_; }

  void StartChunk(ColumnsChunk &chunk) {
    held_.clear();
    if (Exhausted()) Drop();
    chunk.ResetTuples();
  }

  /** Requires a latch */
  void Load() {
    byte *data = Data();
    rows_ = PaxHeader::TupleCount(data);
    for (size_t i = 0; i < columns_.size(); i++) vecs_[i] = Pax::GetColumnUnsafe(data, columns_[i]);
  }

  /** Requires a latch */
  void Fill(ColumnsChunk &chunk) {
    const u16 n = static_cast<u16>(
        std::min<u32>(STANDARD_VECTOR_SIZE - chunk.GetTupleCount(), rows_ - start_));
    for (u16 i = 0; i < columns_.size(); i++) chunk.AppendColumnDataByIdx(i, vecs_[i], start_, n);
    chunk.AdvanceTupleCount(n);
    start_ += n;
  }

  u16 Position() const { return start_; }
};

enum class ScanState : u8 {
  START_CHUNK, // previous chunk was consumed: reset it
  FILL,        // copy rows from the current page
  NEED_PAGE,   // ask the stream for the next page
  WAIT_IO,     // page requested, waiting for its I/O to finish
  DRAIN,       // table finished: hand out the last partial chunk
  FINISHED,
};

class ScanLocal {
private:
  ParallelScanShared &shared_;
  AccessStrategy strategy_;
  ReadIoStream<ScanCursor> stream_;
  ColumnsChunk chunk_;
  PageCursor page_cursor_;
  ScanState state_ = ScanState::START_CHUNK;

  static ColumnsChunk MakeChunk(const std::vector<StorageColumnDesc> &columns) {
    std::vector<ChunkColumn> cols;
    cols.reserve(columns.size());
    for (const auto &c : columns) cols.emplace_back(c.column_id, c.slot_size);
    return ColumnsChunk(cols);
  }

public:
  ScanLocal(BufferPool *pool, ParallelScanShared &shared, u32 max_distance)
      : shared_(shared), strategy_(BULK_READ),
        stream_(pool, strategy_, shared.tbl_id, ScanCursor(shared), max_distance),
        chunk_(MakeChunk(shared_.columns_)), page_cursor_(shared_.columns_) {}

  ScanLocal(const ScanLocal &) = delete;
  ScanLocal &operator=(const ScanLocal &) = delete;
  ~ScanLocal() = default;

  ReadIoStream<ScanCursor> &Stream() { return stream_; }
  PageCursor &Cursor() { return page_cursor_; }
  ColumnsChunk &Chunk() { return chunk_; }
  const std::vector<StorageColumnDesc> &Columns() const { return shared_.columns_; }
  ScanState &State() { return state_; }
  transaction::TransactionContext &Context() const { return shared_.context; }
};
} // namespace db7::storage