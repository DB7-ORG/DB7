#pragma once

#include "catalog/entries/table_catalog_entry_base.hpp"
#include "catalog/objects/alter_table_info.hpp"
#include "catalog/objects/create_table_info.hpp"

namespace db7::catalog {
class TableCatalogEntry : public TableCatalogEntryBase {
public:
  std::shared_ptr<StorageTable> storage;
  // //! Manages dependencies of the individual columns of the table
  // ColumnDependencyManager column_dependency_manager;

  TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntryBase &schema, CreateTableInfo &info,
                    std::shared_ptr<StorageTable> inherited_storage = nullptr);

  const std::vector<std::unique_ptr<Constraint>> &GetConstraints() const;

  std::unique_ptr<CatalogEntry> AlterEntry(transaction::TransactionContext &context,
                                           AlterInfo &info);

  std::unique_ptr<CatalogEntry> AddForeignKeyConstraint(AlterForeignKeyInfo &info);
};
} // namespace db7::catalog