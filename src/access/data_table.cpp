#include "access/data_table.hpp"
#include "access/index/btree.hpp"
#include "access/objects/add_table_index_info.hpp"
#include "shared/error/exception.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/data_chunk.hpp"
#include "storage/fsm/fsm_varlen.hpp"
#include "storage/layouts/pax.hpp"

namespace db7::access {

DataTable::DataTable(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng,
                     std::vector<storage::StorageColumnDesc> &columns, column_t ncols, idx_t oid)
    : buffer_(buffer), disk_mng_(disk_mng), columns_(std::move(columns)), ncols_(ncols),
      initializer_(DataChunk::BuildDataChunkInitializer(columns_)), oid_(oid) {
  for (const auto &col : columns_) {
    if (!col.isDropped) { valid_columns_.push_back(col); }
  }
}

void DataTable::AddIndex(std::unique_ptr<AddTableIndexInfo> info) {
  indexes_.push_back(std::move(info));
}

storage::Page *DataTable::GetPageForInsert(DataChunk *chunk) {
  u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk->GetSize());
  storage::PageIdentifier id(oid_, page_id);
  storage::Page *insert_page = buffer_->Pin(id);
  insert_page->WaitIO();
  return insert_page;
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

// TODO storage insert undo
bool DataTable::Insert(transaction::TransactionContext &context, DataChunk *chunk) {
  u16 required_heap = storage::Pax::CalcHeapBytes(columns_, chunk);
  auto *page = GetPageForInsert(chunk); // TODO storage should pass required_heap here

  page->WDataLock();

  u16 tuple_count = storage::Pax::CurrentRowCount(page->GetData());

  storage::PaxInsertResponse result =
      storage::Pax::Insert(page->GetData(), chunk, valid_columns_, required_heap, ncols_);

  InsertUndo(context, tuple_count, page);

  page->WDataUnlock();

  // TODO storage check unique constraint (LOOK AT OTHER DB CODE)
  for (const auto &index : indexes_) {

    auto &btree = index->index_->Cast<BTreeIndex<>>();
    ResultObj<void> res;
    switch (index->type_) {
    case catalog::IndexConstraintType::NONE:
      res = btree.Insert(chunk, {tuple_count, page->GetPageId()});
    case catalog::IndexConstraintType::PRIMARY:
    case catalog::IndexConstraintType::UNIQUE:
    case catalog::IndexConstraintType::FOREIGN:
      res = btree.InsertUnique(&context, chunk, {tuple_count, page->GetPageId()});
    }

    if (!res.success) {
      throw EXECUTION_EXCEPTION(res.message, shared::ErrorCode::ERRCODE_UNIQUE_VIOLATION);
    }
  }

  (void)result;
  // TODO storage once fsm is added this will need to be handled

  buffer_->Unpin(page, true);

  return result == storage::PaxInsertResponse::OK;
}

} // namespace db7::access