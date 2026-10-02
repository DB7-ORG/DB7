#pragma once

#include "catalog/entries/catalog_entry.hpp"
#include "catalog/entry_lookup_info.hpp"
#include "transaction/transaction_context.hpp"

namespace db7::catalog {

class SchemaCatalogEntry : public InCatalogEntry {
public:
  virtual optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context, const EntryLookupInfo &lookup_info) = 0;
  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context, CatalogType type, const Identifier &name);
};
} // namespace db7::catalog