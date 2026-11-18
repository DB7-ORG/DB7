#pragma once

#include "access/objects/add_table_index_info.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/database_catalog.hpp"
#include "storage/columns_chunk.hpp"
#include "storage/layouts/pax.hpp"
#include "storage/layouts/storage_column_desc.hpp"
#include "storage/models/parallel_scan_shared.hpp"
#include "storage/models/scan_local.hpp"
#include "transaction/transaction_context.hpp"

#include <memory>
#include <vector>

// namespace db7::storage
namespace db7 {
class DataChunk;

namespace storage {
class BufferPool;
class DiskManagerAsync;

} // namespace storage

namespace access {
class SchemaColumn;
class Index;

struct PageInsertResult {
  u16 index;
  bool fetch;
};

enum class ScanResult : u8 { HaveMore, Finished, Blocked };

class DataTable {
private:
  catalog::DatabaseCatalog &catalog_;
  storage::StorageManager &storage_manager_;
  std::vector<storage::StorageColumnDesc> main_columns_;
  std::vector<storage::StorageColumnDesc> valid_columns_;
  DataChunkPtr initializer_;
  std::vector<std::unique_ptr<AddTableIndexInfo>> indexes_;
  table_id oid_; // table id

  enum class FkCheck { Insert, Delete };

private:
  /* Basically version of the current schema or columns_ vector size. */
  size_t NCols() const;

  storage::Page *GetPageForInsert(DataChunk *chunk);

  void InsertUndo(transaction::TransactionContext &context, u16 index, storage::Page *page);

  void VerifyForeignKeyConstraint(transaction::TransactionContext &context,
                                  catalog::ForeignKeyConstraint &constraint, DataChunk *chunk,
                                  FkCheck type);

  PageInsertResult TryInsertInternalHeap(transaction::TransactionContext &context,
                                         storage::Page *page, DataChunk *chunk, u16 required_heap);

  void InsertInternalIndexConstraints(transaction::TransactionContext &context, DataChunk *chunk,
                                      TupleId tuple);

  void InsertInternalIndexOther(DataChunk *chunk, TupleId tuple);

  storage::Page *GetPageById(page_id pid);

  bool DeleteUndo(transaction::TransactionContext &context, u16 index, storage::Page *page);

  bool UpdateUndo(transaction::TransactionContext &context, u16 index, storage::Page *page,
                  DataChunk *chunk);

public:
  DataTable(catalog::DatabaseCatalog &catalog, std::vector<storage::StorageColumnDesc> columns,
            table_id oid_);

  const DataChunk *Initializer() const;

  const DataChunkPtr Initializer(std::vector<storage::StorageColumnDesc> &columns) const;

  const DataChunkPtr Initializer(std::vector<ChunkColumn> &columns) const;

  table_id GetTableId() const;

  /**
   * Appends an index to the list
   * @param info Index info along with the type of constraint(PK, FK ...)
   */
  void AddIndex(std::unique_ptr<AddTableIndexInfo> info);

  /**
   * Inserts a record to table along with indexes on this table
   * while making sure no constraints are violated
   * @param table Table from which we are reading constraints(should be table u see under MVCC)
   * @param chunk Abstraction used to pass data around
   */
  bool Insert(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
              DataChunk *chunk);

  /**
   * Deletes a record to table along with indexes on this table
   * while making sure no constraints are violated
   * @param table Table from which we are reading constraints(should be table u see under MVCC)
   * @param tuple Location which row to delete
   */
  bool Delete(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
              TupleId tuple);

  /**
   * Updates a record to table along with indexes on this table
   * while making sure no constraints are violated.
   * Updates are done in place or using delete + insert if
   * updated fields contain indexes
   * @param table Table from which we are reading constraints(should be table u see under MVCC)
   * @param chunk Abstraction used to pass data around
   * @param tuple Location which row to update
   */
  bool Update(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
              TupleId tuple, DataChunk *chunk);

  static constexpr u16 DEFAULT_MORSEL_SIZE = 64;
  // snapshot nblocks, chunk sizing
  std::unique_ptr<storage::ParallelScanShared>
  InitParallelScan(transaction::TransactionContext &context,
                   std::vector<storage::StorageColumnDesc> &&columns) const;
  // a hint for how many tasks make sense
  idx_t MaxThreads(const storage::ParallelScanShared &state) const;

  std::unique_ptr<storage::ScanLocal> InitLocalScan(storage::ParallelScanShared &shared) const;

  void SelectInto(transaction::TransactionContext &context, ColumnsChunk &chunk,
                  storage::Page *page, u16 page_start, u16 chunk_start, u16 n) const {
    auto *versions = page->GetVersions();
    ChunkBitmap del_bitmap = storage::PaxHeader::DeletedBitmap(page->GetData());

    if (versions == nullptr) {
      for (u16 k = 0; k < n; k++) {
        const u16 row = page_start + k;
        bool visible = !del_bitmap.IsNull(row);
        if (visible) { chunk.AppendSel(chunk_start + k); }
      }
      return;
    }

    for (u16 k = 0; k < n; k++) {
      const u16 row = page_start + k;
      if (del_bitmap.IsNull(row)) continue; // GC'd: dead for every snapshot

      bool visible = true;
      auto *v = versions[row].Get();
      while (v != nullptr && transaction::TransactionUtil::HasConflict(
                                 v->GetTimestamp(), context.FinishTime(), context.StartTime())) {
        switch (v->GetType()) {
        case storage::DeltaRecordType::UPDATE: {
          DataChunk *delta = reinterpret_cast<DataChunk *>(v->GetDelta());
          chunk.EmplaceDataChunk(chunk_start + k, delta, main_columns_);
          break;
        }
        case storage::DeltaRecordType::INSERT: visible = false; break;
        case storage::DeltaRecordType::DELETE: break;
        case storage::DeltaRecordType::INVALID: break;
        }
        if (!visible) { break; }
        v = v->GetNext();
      }
      if (v != nullptr && v->GetType() == storage::DeltaRecordType::DELETE) { continue; }
      if (visible) { chunk.AppendSel(chunk_start + k); }
    }
  }

  // TODO should benchmark blocking vs yielding threads after implementing features
  ScanResult ScanInternal(storage::ScanLocal &local) const {
    auto &stream = local.Stream();
    auto &cursor = local.Cursor();
    auto &chunk = local.Chunk();
    auto &context = local.Context();
    const auto &columns = local.Columns();
    using storage::ScanState;
    ScanState &state = local.State();
    for (;;) {
      switch (state) {
      case ScanState::START_CHUNK: {
        cursor.StartChunk(chunk);
        state = ScanState::FILL;
        break;
      }
      case ScanState::FILL:
        if (!chunk.HasSpace()) {
          state = ScanState::START_CHUNK;
          return ScanResult::HaveMore;
        }
        if (cursor.Exhausted()) {
          state = ScanState::NEED_PAGE;
          break;
        }
        {
          shared::AdaptiveVersionLock::ReadGuard guard(cursor.Page()->DataLock());
          u16 old_start = cursor.Position();
          u16 chunk_start = chunk.GetTupleCount();
          cursor.Fill(chunk);
          u16 new_start = cursor.Position();
          auto &page = cursor.Page();
          SelectInto(context, chunk, page, old_start, chunk_start, new_start - old_start);
        }
        break;

      case ScanState::NEED_PAGE: {
        storage::PinnedPage page = stream.Next();
        if (!page) {
          state = ScanState::DRAIN;
          break;
        }
        cursor.Assign(std::move(page));
        state = ScanState::WAIT_IO;
        break;
      }
      case ScanState::WAIT_IO:
        if (cursor.IsBlocked()) { return ScanResult::Blocked; }
        {
          shared::AdaptiveVersionLock::ReadGuard guard(cursor.Page()->DataLock());
          if (storage::Pax::IsPageReadable(cursor.Data(), columns, NCols())) {
            cursor.Load();
            state = ScanState::FILL;
            break;
          }
        }
        cursor.Drop();
        state = ScanState::NEED_PAGE;
        break;
      case ScanState::DRAIN: {
        state = ScanState::FINISHED;
        if (chunk.GetTupleCount() > 0) return ScanResult::HaveMore;
        break;
      }
      case ScanState::FINISHED: {
        cursor.StartChunk(chunk);
        return ScanResult::Finished;
      }
      }
    }
  }

  ScanResult Scan(storage::ScanLocal &local) const {
    const auto result = ScanInternal(local);
    return result;
  }
};
} // namespace access
} // namespace db7