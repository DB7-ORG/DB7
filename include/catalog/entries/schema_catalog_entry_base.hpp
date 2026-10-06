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
  //! Scan the specified catalog set, invoking the callback method for every entry
  virtual void Scan(transaction::TransactionContext &context, CatalogType type,
                    const std::function<void(CatalogEntry &)> &callback) = 0;
  //! Scan the specified catalog set, invoking the callback method for every committed entry
  virtual void Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) = 0;
};
} // namespace db7::catalog