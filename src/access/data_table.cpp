#include "access/data_table.hpp"
#include "access/objects/add_table_index_info.hpp"
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

// TODO storage insert undo
bool DataTable::Insert(transaction::TransactionContext &context, DataChunk *chunk) {
  u16 required_heap = storage::Pax::CalcHeapBytes(columns_, chunk);
  auto *page = GetPageForInsert(chunk); // TODO storage should pass required_heap here
  page->WDataLock();
  storage::PaxInsertResponse result =
      storage::Pax::Insert(page->GetData(), chunk, valid_columns_, required_heap, ncols_);
  page->WDataUnlock();

  (void)result;
  // TODO storage once fsm is added this will need to be handled

  return result == storage::PaxInsertResponse::OK;
}

} // namespace db7::access