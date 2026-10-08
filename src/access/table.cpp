#include "access/data_table.hpp"
#include "access/objects/add_table_index_info.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/data_chunk.hpp"
#include "storage/fsm/fsm_varlen.hpp"

namespace db7::access {

DataTable::DataTable(storage::BufferPool *buffer, storage::DiskManagerAsync *disk_mng,
                     catalog::ColumnList &columns, idx_t oid)
    : buffer_(buffer), disk_mng_(disk_mng), columns_(std::move(columns)), oid_(oid) {};

void DataTable::AddIndex(std::unique_ptr<AddTableIndexInfo> info) {
  indexes_.push_back(std::move(info));
}

storage::Page *DataTable::GetPageForInsert(DataChunk &chunk) {
  u32 page_id = storage::FreeSpaceManagerVarlen::Get(chunk.GetSize());
  storage::PageIdentifier id(oid_, page_id);
  storage::Page *insert_page = buffer_->Pin(id);
  insert_page->WaitIO();
  return insert_page;
}

bool DataTable::Insert(transaction::TransactionContext &context, DataChunk &chunk) {
  auto *page = GetPageForInsert(chunk);
  page->WDataLock();
  // page should have info about columns i have
  // should have info about offsets for each column
  //

  page->WDataUnlock();
}

} // namespace db7::access