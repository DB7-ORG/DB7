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
    : catalog_(catalog), storage_manager_(catalog.StorageManager()),
      main_columns_(std::move(columns)), oid_(oid) {
  for (const auto &col : main_columns_) {
    if (!col.isDropped) { valid_columns_.push_back(col); }
  }
  initializer_ = DataChunkPtr(DataChunk::BuildDataChunkInitializer(valid_columns_));

  // creates a file for the heap
  auto &disk_mng_ = storage_manager_.GetDiskManager();
  if (!disk_mng_.CreateOpenFile(oid_, 1)) { throw IO_EXCEPTION("Could not create/open file"); }
}

size_t DataTable::NCols() const {
  DB7_ASSERT(main_columns_.size() <= UINT16_MAX, "Too many columns");
  return main_columns_.size();
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

const DataChunkPtr DataTable::Initializer(std::vector<storage::StorageColumnDesc> &columns) const {
  DataChunkPtr ptr(DataChunk::BuildDataChunkInitializer(columns));
  return ptr;
}

const DataChunkPtr DataTable::Initializer(std::vector<ChunkColumn> &columns) const {
  DataChunkPtr ptr(DataChunk::BuildDataChunkInitializer(columns));
  return ptr;
}

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
    const auto &col = main_columns_[key];
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
    const auto &col = main_columns_[src_key];
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
 * @warning make sure to hold the page data lock like with other columns
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

/* Should hold a latch */
PageInsertResult DataTable::TryInsertInternalHeap(transaction::TransactionContext &context,
                                                  storage::Page *page, DataChunk *chunk,
                                                  u16 required_heap) {
  byte *data = page->GetData();
  auto result = storage::Pax::IsPageWritable(data, chunk, NCols(), required_heap);
  switch (result) {
  case storage::PaxInsertResponse::NOT_INITIALIZED: {
    storage::Pax::Init(data, main_columns_, required_heap, required_heap);
    break;
  }
  case storage::PaxInsertResponse::OK: break;
  case storage::NO_SPACE_HEAP:
  case storage::NO_SPACE_CAPACITY:
  case storage::PAGE_SCHEMA_OLDER: return {0, true};
  case storage::WRITER_SCHEMA_STALE:
  case storage::COLUMN_DROPPED: // TODO storage should find another page or abort
  default: throw CATALOG_EXCEPTION("Switch not populated");
  }

  u16 tuple_index = storage::Pax::InsertUnsafe(page->GetData(), chunk, valid_columns_);

  InsertUndo(context, tuple_index, page);

  page->SetDirty();

  return {tuple_index, false};
}

void DataTable::InsertInternalIndexConstraints(transaction::TransactionContext &context,
                                               DataChunk *chunk, TupleId tuple) {
  // Verifies unique constraints and inserts to those indexes
  // (since those indexes can throw verify them first)
  for (const auto &index : indexes_) {
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    switch (index->type_) {
    case catalog::IndexConstraintType::PRIMARY:
    case catalog::IndexConstraintType::UNIQUE: {
      auto res = btree.InsertUnique(&context, chunk, tuple);
      if (!res.success) {
        throw EXECUTION_EXCEPTION(res.message, shared::ErrorCode::ERRCODE_UNIQUE_VIOLATION);
      }
      break;
    }
    case catalog::IndexConstraintType::FOREIGN: {
      auto res = btree.Insert(chunk, tuple);
      if (!res.success) { throw CATALOG_EXCEPTION("Failed to insert to index"); }
      break;
    }
    default: break;
    }
  }
}

void DataTable::InsertInternalIndexOther(DataChunk *chunk, TupleId tuple) {
  // Inserts to other indexes at the end
  for (const auto &index : indexes_) {
    auto &btree = index->index_->Cast<BTreeIndex<>>();
    switch (index->type_) {
    case catalog::IndexConstraintType::NONE: {
      auto res = btree.Insert(chunk, tuple);
      if (!res.success) { throw CATALOG_EXCEPTION("Failed to insert to index"); }
      break;
    }
    default: break;
    }
  }
}

// Every write modifies the row first (under the page latch), and only then reads the current index
// list. This assures online index creation is safe.
bool DataTable::Insert(transaction::TransactionContext &context, catalog::TableCatalogEntry &table,
                       DataChunk *chunk) {
  u16 required_heap = storage::Pax::CalcHeapBytes(main_columns_, chunk);
  auto *page = GetPageForInsert(chunk); // TODO storage should pass required_heap here

  PageInsertResult insert_res;
  const auto page_id = page->GetPageId();
  { // Inserts into the heap
    storage::PagePinGuard ping_(page);
    storage::PageWriteGuard lockg_(page);
    insert_res = TryInsertInternalHeap(context, page, chunk, required_heap);
  }

  if (insert_res.fetch) { throw CATALOG_EXCEPTION("Fetch new page please"); }

  const TupleId tuple = {insert_res.index, page_id};
  InsertInternalIndexConstraints(context, chunk, tuple);

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

  InsertInternalIndexOther(chunk, tuple);

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

    bool result = DeleteUndo(context, tuple.GetIndex(), page);
    if (!result) { return false; }

    /* No need to check is page safe since we know my snapshot can see this */
    chunk.reset(storage::Pax::GetUnsafe(page->GetData(), tuple.GetIndex(), valid_columns_, NCols(),
                                        initializer_.get()));
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

    if (HasConflict(context, version_ptr)) {
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
  page_id page_id = page->GetPageId();

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
  for (u16 i = 0; i < chunk->GetColCount(); i++) {
    DB7_ASSERT(!main_columns_[column_ids[i]].isDropped, "Dropped column access");
    projected.push_back(main_columns_[column_ids[i]]);
  }
  DB7_ASSERT(chunk->GetColCount() == projected.size(), "Size mismatch");

  DataChunkPtr current;
  DataChunkPtr old_row;
  PageInsertResult insert_res;
  u16 required_chunk_heap = storage::Pax::CalcHeapBytes(main_columns_, chunk);
  {
    storage::PagePinGuard ping_(page);
    storage::PageWriteGuard lockg_(page);

    bool update_in_place =
        !contains_col_index &&
        storage::Pax::IsPageUpdatable(page->GetData(), chunk, NCols(), required_chunk_heap) ==
            storage::PaxInsertResponse::OK;

    if (update_in_place) {
      // do in place update
      current.reset(storage::Pax::GetUnsafe(page->GetData(), tuple.GetIndex(), projected, NCols(),
                                            initializer_.get()));

      bool res = UpdateUndo(context, tuple.GetIndex(), page, current.get());
      if (!res) { return false; }

      storage::Pax::UpdateUnsafe(page->GetData(), chunk, projected, tuple.GetIndex());
      page->SetDirty();

      return true;
    }

    // do delete + insert

    bool result = DeleteUndo(context, tuple.GetIndex(), page);
    if (!result) { return false; }

    current.reset(storage::Pax::GetUnsafe(page->GetData(), tuple.GetIndex(), valid_columns_,
                                          NCols(), initializer_.get(), chunk->GetHeapSize()));

    old_row.reset(current->Copy());

    // Merge 2 chunks
    current->MergeInto(chunk, main_columns_);

    u16 required_heap = storage::Pax::CalcHeapBytes(main_columns_, current.get());

    insert_res = TryInsertInternalHeap(context, page, current.get(), required_heap);
  }

  const TupleId inserted_tuple = {insert_res.index, page_id};
  if (insert_res.fetch) { throw CATALOG_EXCEPTION("Please provide another page tnx"); }

  shared::BitSet changed_set =
      storage::ChunkDiff::ChangedColumns(old_row.get(), chunk, main_columns_);

  InsertInternalIndexConstraints(context, current.get(), inserted_tuple);

  if (contains_col_index) {
    const auto &constraints = table.GetConstraints();
    for (const auto &constraint : constraints) {
      switch (constraint->type) {
      case catalog::ConstraintType::FOREIGN_KEY: {
        auto &fk = constraint->Cast<catalog::ForeignKeyConstraint>();
        auto any_changed = [&](const auto &keys) {
          for (const auto k : keys)
            if (changed_set.Contains(k)) return true;
          return false;
        };
        if (fk.info.IsAppendConstraint() && any_changed(fk.info.fk_keys)) {
          VerifyForeignKeyConstraint(context, fk, current.get(),
                                     FkCheck::Insert); // new value must exist
        }
        if (fk.info.IsDeleteConstraint() && any_changed(fk.info.pk_keys)) {
          VerifyForeignKeyConstraint(context, fk, old_row.get(),
                                     FkCheck::Delete); // old value must be free
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
  }

  InsertInternalIndexOther(current.get(), inserted_tuple);

  return true;
}

} // namespace db7::access