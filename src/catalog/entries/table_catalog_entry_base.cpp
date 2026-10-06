#include "catalog/entries/table_catalog_entry_base.hpp"

namespace db7::catalog {
TableCatalogEntryBase::TableCatalogEntryBase(DatabaseCatalog &catalog, SchemaCatalogEntry &schema,
                                             CreateTableInfo &info)
    : StandardEntry(CatalogType::TABLE_ENTRY, schema, catalog, info.table),
      columns(std::move(info.columns)), constraints(std::move(info.constraints)) {
  this->dependencies = info.dependencies;
}
} // namespace db7::catalog