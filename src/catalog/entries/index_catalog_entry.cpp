#include "catalog/entries/index_catalog_entry.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/entries/table_catalog_entry.hpp"

namespace db7::catalog {
IndexCatalogEntry::IndexCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntry &schema,
                                     CreateIndexInfo &create_info, TableCatalogEntry &table_p)
    : IndexCatalogEntryBase(catalog, schema, create_info) {}
} // namespace db7::catalog