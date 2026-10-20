#pragma once

#include "access/objects/add_table_index_info.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/database_catalog.hpp"
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
};
} // namespace access
} // namespace db7