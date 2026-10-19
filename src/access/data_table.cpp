#include "access/data_table.hpp"
#include "access/index/btree.hpp"
#include "access/objects/add_table_index_info.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
#include "shared/error/exception.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/data_chunk.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "storage/layouts/pax.hpp"
#include "storage/storage_manager.hpp"

#include <utility>

namespace db7::access {

DataTable::DataTable(catalog::DatabaseCatalog &catalog,
                     std::vector<storage::StorageColumnDesc> columns, table_id oid)
    : catalog_(catalog), storage_manager_(catalog.StorageManager()), columns_(std::move(columns)),
      oid_(oid) {
  for (const auto &col : columns_) {
    if (!col.isDropped) { valid_columns_.push_back(col); }
  }
  initializer_ = DataChunkPtr(DataChunk::BuildDataChunkInitializer(valid_columns_));

  // creates a file for the heap
  auto &disk_mng_ = storage_manager_.GetDiskManager();
  if (!disk_mng_.CreateOpenFile(oid_, 1)) { throw IO_EXCEPTION("Could not create/open file"); }
}

storage::Page *DataTable::GetPageForInsert(DataChunk *chunk) {
  u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk->GetSize());
  storage::PageIdentifier id(oid_, page_id);
  storage::Page *insert_page = storage_manager_.GetBufferPool().Pin(id);
  insert_page->WaitIO();
  return insert_page;
}

storage::Page *DataTable::GetPageById(page_id pid) {
  storage::PageIdentifier id(oid_, pid);
  storage::Page *page = storage_manager_.GetBufferPool().Pin(id);
  page->WaitIO();
  return page;
}

const DataChunk *DataTable::Initializer() const { return initializer_.get(); }

table_id DataTable::GetTableId() const { return oid_; }

void DataTable::AddIndex(std::unique_ptr<AddTableIndexInfo> info) {
  indexes_.push_back(std::move(info));
}

// TODO storage likely can be optimized by removing write/get by oid
void DataTable::VerifyForeignKeyConstraint(transaction::TransactionContext &context,
                                           catalog::ForeignKeyConstraint &constraint,
                                           DataChunk *chunk, FkCheck type) {

  const bool is_insert = type == FkCheck::Insert;

  const auto &src_keys = is_insert ? constraint.info.fk_keys : constraint.info.pk_keys;
  const auto &dst_keys = is_insert ? constraint.info.pk_keys : constraint.info.fk_keys;
  DB7_ASSERT(src_keys.size() == dst_keys.size(), "Should be same size");

  optional_ptr<catalog::CatalogEntry> catalog_entry = catalog_.GetEntry(
      context, constraint.info.schema, {catalog::CatalogType::TABLE_ENTRY, constraint.info.table});
  if (!catalog_entry) { throw CATALOG_EXCEPTION("Not found"); }
  auto &table_entry = catalog_entry->Cast<catalog::TableCatalogEntry>();
  auto storage = table_entry.storage.get();
  const DataChunk *builder = storage->Initializer();

  /* Calculates required size for varlen heap of data chunk */
  size_t size = 0;
  for (const auto key : src_keys) {
    const auto &col = columns_[key];
    DB7_ASSERT(!col.isDropped, "Dropped column in use");
    byte *slot = chunk->GetByOidNullCheck(key);
    // if any column is null for a fk check there is no need to probe the index
    if (slot == nullptr) { return; }
    if (!col.isVarlen) { continue; }
    size += storage::VarlenEntry::Cast(slot)->HeapSize();
  }

  /* Allocates a chunk */
  DataChunkPtr dst_chunk(DataChunk::BuildDataChunk(builder, size));

  /* Fills the chunk with data needed for current index we are checking */
  for (u16 i = 0; i < src_keys.size(); i++) {
    const auto src_key = src_keys[i];
    const auto dst_key = dst_keys[i];
    const auto &col = columns_[src_key];
    byte *slot = chunk->GetByOid(src_key);
    if (col.isVarlen) {
      auto varlen = storage::VarlenEntry::Cast(slot)->Data(chunk->Bytes());
      dst_chunk->WriteVarlenByOid(dst_key, varlen);
    } else {
      dst_chunk->WriteByOid(dst_key, {slot, col.slot_size});
    }
  }

  bool valid = false;
  for (auto &index : storage->indexes_) {
    if (is_insert && index->type_ != catalog::IndexConstraintType::UNIQUE &&
        index->type_ != catalog::IndexConstraintType::PRIMARY) {
      continue;
    }
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    bool is_matching_index = btree.CheckIndexColumns(dst_keys);
    if (!is_matching_index) { continue; }

    shared::VectorValues<TupleId> results;
    auto res = btree.Get(dst_chunk.get(), results);
    if (!res.success) { throw CATALOG_EXCEPTION(res.message); }

    ResultObj<TupleId> r = context.GetFirstVisibleTid(results.vec, storage->GetTableId());
    if (is_insert && !r.success) {
      throw CATALOG_EXCEPTION("FK violation: referenced key not found");
    }
    if (!is_insert && r.success) {
      throw CATALOG_EXCEPTION("FK violation: key is still referenced");
    }

    valid = true;
  }

  if (!valid) { throw CATALOG_EXCEPTION("Unreachable code: No matching index for fk check"); }
}

/**
 * @warning make sure to hold the page data lock like w other columns
 */
void DataTable::InsertUndo(transaction::TransactionContext &context, u16 index,
                           storage::Page *page) {
  u32 max_tuple_count = storage::Pax::MaxRowCount(page->GetData());

  auto page_id = page->GetPageId();

  storage::UndoRecord *record = context.UndoRecordForInsert(oid_, page_id, index);

  storage::VersionPtr *versions =
      context.GetVersions(page, storage::PageIdentifier{oid_, page_id}, max_tuple_count);

  versions[index].Set(record);
}

// Every write modifies the row first (under the page latch), and only then reads the current index
// list. This assures online index creation is safe.
bool DataTable::Insert(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
                       DataChunk *chunk) {
  u16 required_heap = storage::Pax::CalcHeapBytes(columns_, chunk);
  auto *page = GetPageForInsert(chunk); // TODO storage should pass required_heap here

  u16 tuple_index;
  const auto page_id = page->GetPageId();
  byte *data = page->GetData();
  { // Inserts into the heap
    storage::PagePinGuard ping_(page);
    storage::PageWriteGuard lockg_(page);

    if (!storage::Pax::IsInitialized(data)) {
      storage::Pax::Init(data, columns_, required_heap, required_heap);
    }

    tuple_index = storage::Pax::CurrentRowCount(data);

    storage::PaxInsertResponse result = storage::Pax::Insert(page->GetData(), chunk, valid_columns_,
                                                             required_heap, columns_.size());
    // TODO storage once fsm is added this will need to be handled
    if (result != storage::PaxInsertResponse::OK) { return false; }
    page->SetDirty();

    InsertUndo(context, tuple_index, page);
  }

  // Verifies unique constraints and inserts to those indexes
  // (since those indexes can throw verify them first)
  for (const auto &index : indexes_) {
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    ResultObj<void> res;
    switch (index->type_) {
    case catalog::IndexConstraintType::PRIMARY:
    case catalog::IndexConstraintType::UNIQUE: {
      res = btree.InsertUnique(&context, chunk, {tuple_index, page_id});
      if (!res.success) {
        throw EXECUTION_EXCEPTION(res.message, shared::ErrorCode::ERRCODE_UNIQUE_VIOLATION);
      }
      break;
    }
    case catalog::IndexConstraintType::FOREIGN: {
      btree.Insert(chunk, {tuple_index, page_id});
      break;
    }
    default: break;
    }
  }

  const auto &constraints = table.GetConstraints();
  for (const auto &constraint : constraints) {
    switch (constraint->type) {
    case catalog::ConstraintType::FOREIGN_KEY: {
      auto &fk_constraint = constraint->Cast<catalog::ForeignKeyConstraint>();
      if (fk_constraint.info.IsAppendConstraint()) {
        VerifyForeignKeyConstraint(context, fk_constraint, chunk, FkCheck::Insert);
      }
      break;
    }
    case catalog::ConstraintType::CHECK: break;    // TODO storage
    case catalog::ConstraintType::NOT_NULL: break; // TODO storage
    case catalog::ConstraintType::UNIQUE: break;   // already checked above
    case catalog::ConstraintType::INVALID:
      throw EXECUTION_EXCEPTION("Invalid constraint type",
                                shared::ErrorCode::ERRCODE_FEATURE_NOT_SUPPORTED);
    }
  }

  // Inserts to other indexes at the end
  for (const auto &index : indexes_) {
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    switch (index->type_) {
    case catalog::IndexConstraintType::NONE: {
      btree.Insert(chunk, {tuple_index, page_id});
      break;
    }
    default: break;
    }
  }

  return true;
}

bool HasConflict(transaction::TransactionContext &context, storage::UndoRecord *version_ptr) {
  /* Nobody modified this tuple */
  if (version_ptr == nullptr) return false;

  return version_ptr->IsDeleted() ||
         transaction::TransactionUtil::HasConflict(version_ptr->GetTimestamp(),
                                                   context.FinishTime(), context.StartTime());
}

/**
 * @warning make sure to hold the page data lock like w other columns
 */
bool DataTable::DeleteUndo(transaction::TransactionContext &context, u16 index,
                           storage::Page *page) {
  u32 max_tuple_count = storage::Pax::MaxRowCount(page->GetData());

  auto page_id = page->GetPageId();

  storage::UndoRecord *record = context.UndoRecordForDelete(oid_, page_id, index);

  storage::VersionPtr *versions =
      context.GetVersions(page, storage::PageIdentifier{oid_, page_id}, max_tuple_count);

  storage::UndoRecord *version_ptr;

  do {
    version_ptr = versions[index].Get();

    if (HasConflict(context, version_ptr)) {
      record->Invalidate();
      return false;
    }

    record->SetNext(version_ptr);

  } while (!versions[index].CompareAndSwap(version_ptr, record));

  return true;
}

bool DataTable::Delete(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
                       TupleId tuple) {

  auto *page = GetPageById(tuple.GetPageId());

  DataChunkPtr chunk; // RAII guard for cleanup
  {
    storage::PagePinGuard ping_(page);
    storage::PageWriteGuard lockg_(page);
    chunk.reset(storage::Pax::Get(page->GetData(), tuple.GetIndex(), valid_columns_,
                                  columns_.size(), initializer_.get()));
    bool result = DeleteUndo(context, tuple.GetIndex(), page);
    if (!result) { return false; }
  }

  const auto &constraints = table.GetConstraints();
  for (const auto &constraint : constraints) {
    switch (constraint->type) {
    case catalog::ConstraintType::FOREIGN_KEY: {
      auto &fk_constraint = constraint->Cast<catalog::ForeignKeyConstraint>();
      if (fk_constraint.info.IsDeleteConstraint()) {
        VerifyForeignKeyConstraint(context, fk_constraint, chunk.get(), FkCheck::Delete);
      }
      break;
    }
    case catalog::ConstraintType::CHECK: break;
    case catalog::ConstraintType::NOT_NULL: break;
    case catalog::ConstraintType::UNIQUE: break;
    case catalog::ConstraintType::INVALID:
      throw EXECUTION_EXCEPTION("Invalid constraint type",
                                shared::ErrorCode::ERRCODE_FEATURE_NOT_SUPPORTED);
    }
  }

  return true;
}

/**
 * @warning make sure to hold the page data lock like w other columns
 */
bool DataTable::UpdateUndo(transaction::TransactionContext &context, u16 index, storage::Page *page,
                           DataChunk *prev) {
  u32 max_tuple_count = storage::Pax::MaxRowCount(page->GetData());

  page_id pid = page->GetPageId();

  storage::UndoRecord *record = context.UndoRecordForUpdate(oid_, pid, index, prev);

  storage::VersionPtr *versions =
      context.GetVersions(page, storage::PageIdentifier{oid_, pid}, max_tuple_count);

  storage::UndoRecord *version_ptr;
  do {
    version_ptr = versions[index].Get();

    if (version_ptr != nullptr && (HasConflict(context, version_ptr))) {
      record->Invalidate();
      return false;
    }

    record->SetNext(version_ptr);

  } while (!versions[index].CompareAndSwap(version_ptr, record));

  return true;
}

bool DataTable::Update(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
                       TupleId tuple, DataChunk *chunk) {
  auto *page = GetPageById(tuple.GetPageId());

  const auto *column_ids = chunk->ColumnIds();
  shared::BitSet set;
  for (size_t i = 0; i < chunk->GetColCount(); i++) { set.Add(column_ids[i]); }

  bool contains_col_index = false;
  for (const auto &index : indexes_) {
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    if (btree.ContainsColumns(set)) {
      contains_col_index = true;
      break;
    }
  }

  std::vector<storage::StorageColumnDesc> projected;
  projected.reserve(chunk->GetColCount());
  for (const auto &col : valid_columns_) {
    if (set.Contains(col.column_id)) { projected.push_back(col); }
  }
  DB7_ASSERT(chunk->GetColCount() == projected.size(), "Size mismatch");

  // TODO storage check other constraints
  const auto &constraints = table.GetConstraints();
  for (const auto &constraint : constraints) {
    switch (constraint->type) {
    case catalog::ConstraintType::FOREIGN_KEY: break;
    case catalog::ConstraintType::CHECK: break;    // TODO storage
    case catalog::ConstraintType::NOT_NULL: break; // TODO storage
    case catalog::ConstraintType::UNIQUE: break;   // already checked above
    case catalog::ConstraintType::INVALID:
      throw EXECUTION_EXCEPTION("Invalid constraint type",
                                shared::ErrorCode::ERRCODE_FEATURE_NOT_SUPPORTED);
    }
  }

  {
    storage::PagePinGuard ping_(page);
    storage::PageWriteGuard lockg_(page);

    bool update_in_place = !contains_col_index &&
                           storage::Pax::IsPageVisible(page->GetData(), projected, columns_.size());
    if (update_in_place) {
      u16 required_heap = storage::Pax::CalcHeapBytes(projected, chunk);
      if (storage::Pax::HasAvailableHeapSpace(page->GetData(), required_heap)) {
        // do in place update
        DataChunkPtr current(storage::Pax::Get(page->GetData(), tuple.GetIndex(), projected,
                                               columns_.size(), initializer_.get()));
        storage::PaxInsertResponse result = storage::Pax::Update(
            page->GetData(), chunk, projected, required_heap, columns_.size(), tuple.GetIndex());
        if (result == storage::PaxInsertResponse::OK) {
          bool res = UpdateUndo(context, tuple.GetIndex(), page, current.get());
          if (!res) { return false; }
          return true;
        }
      }
    }

    // TODO storage delete + insert
    // auto *current = storage::Pax::Get(page->GetData(), tuple.GetIndex(), valid_columns_,
    //                                   columns_.size(), initializer_.get());
    // bool result = DeleteUndo(context, tuple.GetIndex(), page);
    // if (!result) { return false; }
    // bool res = Insert(context, table, current);
    // if (!res) { return false; }
  }

  return true;
}

} // namespace db7::access