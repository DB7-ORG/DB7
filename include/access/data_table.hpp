#pragma once

#include "access/objects/add_table_index_info.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/database_catalog.hpp"
#include "shared/poll_result.hpp"
#include "storage/columns_chunk.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "storage/layouts/storage_column_desc.hpp"
#include "storage/models/parallel_scan_shared.hpp"
#include "storage/models/scan_local.hpp"
#include "transaction/transaction_context.hpp"

#include <memory>
#include <vector>

// namespace db7::storage
namespace db7 {

using ScanResult = PollResult;

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

class DataTable {
private:
  catalog::DatabaseCatalog &catalog_;
  storage::StorageManager &storage_manager_;
  std::vector<storage::StorageColumnDesc> main_columns_;
  std::vector<storage::StorageColumnDesc> valid_columns_;
  DataChunkPtr initializer_;
  std::vector<std::unique_ptr<AddTableIndexInfo>> indexes_;
  table_id oid_; // table id

  // TODO fsm
  storage::FreeSpaceManagerVarlen fsm_;

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

  // TODO should benchmark blocking vs yielding threads after implementing features
  ScanResult ScanInternal(storage::ScanLocal &local) const;

public:
  DataTable(catalog::DatabaseCatalog &catalog, std::vector<storage::StorageColumnDesc> columns,
            table_id oid_);

  /** Just for testing for now */
  const auto &Indexes() const { return indexes_; }

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
                   const std::vector<storage::StorageColumnDesc> columns) const;
  // a hint for how many tasks make sense
  idx_t MaxThreads(const storage::ParallelScanShared &state) const;

  std::unique_ptr<storage::ScanLocal> InitLocalScan(storage::ParallelScanShared &shared) const;

  /**
   * Emplaces correct version into columns chunk. Assumes newsest version is already
   * in the chunk.
   */
  void SelectInto(transaction::TransactionContext &context, ColumnsChunk &chunk,
                  storage::Page *page, u16 page_start, u16 chunk_start, u16 n) const;

  /**
   * Fills the columns chunk up to STANDARD_VECTOR_SIZE rows (copies) and passes it up the stack
   * so operators can work with the data.
   * Requires calling InitParallelScan and InitLocalScan to initialize the state for the
   * scheduler so it can descedule the task safely at some moment.
   *
   * @param local Local state seen by the worker that is currently working on the task.
   */
  ScanResult Scan(storage::ScanLocal &local) const;
};
} // namespace access
} // namespace db7