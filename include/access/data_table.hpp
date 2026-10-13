#pragma once

#include "access/objects/add_table_index_info.hpp"
#include "storage/layouts/storage_column_desc.hpp"
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

class DataTable {
private:
  storage::BufferPool *buffer_;
  storage::DiskManagerAsync *disk_mng_;
  std::vector<storage::StorageColumnDesc> columns_;
  std::vector<storage::StorageColumnDesc> valid_columns_;
  column_t ncols_;
  DataChunkPtr initializer_;
  std::vector<std::unique_ptr<AddTableIndexInfo>> indexes_;
  table_id oid_; // table id

private:
  storage::Page *GetPageForInsert(DataChunk *chunk);
  void InsertUndo(transaction::TransactionContext &context, u16 index, storage::Page *page);

public:
  DataTable(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng,
            std::vector<storage::StorageColumnDesc> &columns, column_t ncols, idx_t oid_);

  /**
   * Appends an index to the list
   * @param info Index info along with the type of constraint(PK, FK ...)
   */
  void AddIndex(std::unique_ptr<AddTableIndexInfo> info);

  /**
   * Inserts a record to table along with indexes on this table
   * while making sure no constraints are violated
   * @param chunk Abstraction used to pass data around
   */
  bool Insert(transaction::TransactionContext &context, DataChunk *chunk);

  /**
   * Updates a record to table along with indexes on this table
   * while making sure no constraints are violated.
   * Updates are done in place or using delete + insert if
   * updated fields contain indexes
   * @param chunk Abstraction used to pass data around
   */
  bool Update(transaction::TransactionContext &context, TupleId tuple, DataChunk *chunk);

  /**
   * Deletes a record to table along with indexes on this table
   * while making sure no constraints are violated
   * @param tuple Location which row to delete
   */
  bool Delete(transaction::TransactionContext &context, TupleId tuple);
};
} // namespace access
} // namespace db7