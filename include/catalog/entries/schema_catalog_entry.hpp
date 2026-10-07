#pragma once

#include "catalog/catalog_set.hpp"
#include "catalog/entries/schema_catalog_entry_base.hpp"
#include "catalog/objects/create_table_info.hpp"

namespace db7::catalog {

class StandardEntry;
class AlterInfo;

class SchemaCatalogEntry : public SchemaCatalogEntryBase {
private:
  //! The catalog set holding the tables
  CatalogSet tables;
  //! The catalog set holding the indexes
  CatalogSet indexes;
  //! The catalog set holding the table functions
  CatalogSet table_functions;
  //! The catalog set holding the copy functions
  CatalogSet copy_functions;
  //! The catalog set holding the pragma functions
  CatalogSet pragma_functions;
  //! The catalog set holding the scalar and aggregate functions
  CatalogSet functions;
  //! The catalog set holding the sequences
  CatalogSet sequences;
  //! The catalog set holding the collations
  CatalogSet collations;
  //! The catalog set holding the types
  CatalogSet types;
  //! The catalog set holding the coordinate systems
  CatalogSet coordinate_systems;

private:
  CatalogSet &GetCatalogSet(CatalogType type);

  optional_ptr<CatalogEntry> AddEntryInternal(transaction::TransactionContext &context,
                                              std::unique_ptr<StandardEntry> entry,
                                              LogicalDependencyList dependencies);

public:
  SchemaCatalogEntry(DatabaseCatalog &catalog, Identifier &schema);

  optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context,
                                         const EntryLookupInfo &lookup_info) override;

  void Alter(transaction::TransactionContext &context, AlterInfo &info);

  optional_ptr<CatalogEntry> CreateTable(transaction::TransactionContext &context,
                                         CreateTableInfo &info) override;

  optional_ptr<CatalogEntry> CreateIndex(transaction::TransactionContext &context,
                                         CreateIndexInfo &info, TableCatalogEntry &table) override;

  void Scan(transaction::TransactionContext &context, CatalogType type,
            const std::function<void(CatalogEntry &)> &callback) override;

  void Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) override;
};
} // namespace db7::catalog