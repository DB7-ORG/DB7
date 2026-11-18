#include "transaction/transaction_context.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::transaction {

void TransactionContext::RestampVersions(timestamp_t commit_time) {
  for (auto &rec : undo_buffer_) {
    if (rec.GetType() == storage::DeltaRecordType::CATALOG) {
      rec.GetCatalogEntry()->timestamp.store(commit_time);
    } else {
      rec.SetTimestamp(commit_time);
    }
  }
}
} // namespace db7::transaction