#pragma once

#include "catalog/entries/catalog_entry.hpp"
#include "catalog/entry_lookup_info.hpp"
#include "catalog/objects/create_table_info.hpp"
#include "transaction/transaction_context.hpp"

namespace db7::catalog {

class SchemaCatalogEntryBase : public InCatalogEntry {
public:
  SchemaCatalogEntryBase(DatabaseCatalog &catalog, Identifier &schema);

  virtual optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context,
                                                 const EntryLookupInfo &lookup_info) = 0;
  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context, CatalogType type,
                                      const Identifier &name);
  virtual optional_ptr<CatalogEntry> CreateTable(transaction::TransactionContext &context,
                                                 CreateTableInfo &info) = 0;
};
} // namespace db7::catalog