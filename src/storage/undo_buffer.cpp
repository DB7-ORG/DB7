#include "storage/undo_buffer.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::storage {
UndoRecord *UndoRecord::InitializeCatalog(byte *head, timestamp_t ts,
                                          catalog::CatalogEntry *entry) {
  auto *r = reinterpret_cast<UndoRecord *>(head);
  r->type_ = DeltaRecordType::CATALOG;
  r->next_ = nullptr;
  r->timestamp_.store(ts);
  r->size_ = AlignedPayload(sizeof(void *));
  std::memcpy(r->varlen_contents_, &entry, sizeof(void *));
  return r;
}

catalog::CatalogEntry *UndoRecord::GetCatalogEntry() {
  catalog::CatalogEntry *e;
  std::memcpy(&e, varlen_contents_, sizeof(void *));
  return e;
}

} // namespace db7::storage