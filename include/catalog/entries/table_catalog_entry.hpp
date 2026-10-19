#pragma once

#include "catalog/entries/table_catalog_entry_base.hpp"
#include "catalog/objects/alter_table_info.hpp"
#include "catalog/objects/create_table_info.hpp"

namespace db7 {
namespace storage {
class BufferPool;
class DiskManagerAsync;
} // namespace storage

namespace catalog {
class TableCatalogEntry : public TableCatalogEntryBase {

private:
  // counter that generates table oids
  static inline std::atomic<table_id> next_oid_{1};
  table_id NextOid();

public:
  std::shared_ptr<access::DataTable> storage;
  // //! Manages dependencies of the individual columns of the table
  // ColumnDependencyManager column_dependency_manager;

  TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntryBase &schema, CreateTableInfo &info,
                    std::shared_ptr<access::DataTable> inherited_storage = nullptr);

  TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntry &schema, CreateTableInfo &info,
                    const access::DataTable &prev);

  const std::vector<std::unique_ptr<Constraint>> &GetConstraints() const;

  std::unique_ptr<CatalogEntry> AlterEntry(transaction::TransactionContext &context,
                                           AlterInfo &info);

  std::unique_ptr<CatalogEntry> AddForeignKeyConstraint(AlterForeignKeyInfo &info);
};
} // namespace catalog
} // namespace db7