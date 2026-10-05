#include "catalog/entries/schema_catalog_entry_base.hpp"
#include "catalog/entries/catalog_entry.hpp"
#include "catalog/entry_lookup_info.hpp"
#include "transaction/transaction_context.hpp"

namespace db7::catalog {
optional_ptr<CatalogEntry>
SchemaCatalogEntryBase::GetEntry(transaction::TransactionContext &context, CatalogType type,
                                 const Identifier &name) {
  EntryLookupInfo lookup_info(type, name);
  return LookupEntry(context, lookup_info);
}

SchemaCatalogEntryBase::SchemaCatalogEntryBase(DatabaseCatalog &catalog, Identifier &schema)
    : InCatalogEntry(CatalogType::SCHEMA_ENTRY, catalog, schema) {}

} // namespace db7::catalog