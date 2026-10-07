#include "catalog/entries/index_catalog_entry_base.hpp"

namespace db7::catalog {
IndexCatalogEntryBase::IndexCatalogEntryBase(DatabaseCatalog &catalog,
                                             SchemaCatalogEntryBase &schema, CreateIndexInfo &info)
    : StandardEntry(CatalogType::INDEX_ENTRY, schema, catalog, info.index_name),
      index_constraint_type(info.constraint_type), column_ids(info.column_ids) {
  this->temporary = info.temporary;
  this->dependencies = info.dependencies;
}
} // namespace db7::catalog