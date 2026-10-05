#pragma once

#include "catalog/entries/table_catalog_entry_base.hpp"
#include "catalog/objects/create_table_info.hpp"

namespace db7::catalog {
class TableCatalogEntry : public TableCatalogEntryBase {
public:
  std::shared_ptr<StorageTable> storage;
  // //! Manages dependencies of the individual columns of the table
  // ColumnDependencyManager column_dependency_manager;

  TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntry schema, BoundCreateTableInfo &info,
                    std::shared_ptr<StorageTable> inherited_storage); // TODO continue here
};
} // namespace db7::catalog