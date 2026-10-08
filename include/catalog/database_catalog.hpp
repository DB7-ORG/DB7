#pragma once

#include "catalog/builder.hpp"
#include "catalog/entries/catalog_entry.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/entry_lookup_info.hpp"
// #include "catalog/objects/create_table_info.hpp"
#include "shared/identifier.hpp"

#include <atomic>
#include <memory>

namespace db7::catalog {

class DependencyManager;
class CreateTableInfo;
class CreateIndexInfo;
class TableCatalogEntry;

class DatabaseCatalog {
private:
  std::mutex mu;
  Identifier name_;
  std::unique_ptr<DependencyManager> dependency_manager_;
  std::unique_ptr<CatalogSet> schemas_;
  std::atomic<idx_t> gen_oid_;

public:
  DatabaseCatalog(Identifier name_);
  ~DatabaseCatalog();

  idx_t NextOid() { return gen_oid_++; }

  const Identifier &GetName() const { return name_; }

private:
  optional_ptr<CatalogEntry> CreateSchemaInternal(transaction::TransactionContext &context,
                                                  Identifier &name);

public:
  std::mutex &GetLock() { return mu; }

  // Finds schema by Identifier from CatalogSet
  optional_ptr<SchemaCatalogEntryBase> LookupSchema(transaction::TransactionContext &context,
                                                    const EntryLookupInfo &schema_lookup,
                                                    OnEntryNotFound if_not_found);

  // Finds schema by Identifier from CatalogSet
  optional_ptr<SchemaCatalogEntryBase> GetSchema(transaction::TransactionContext &context,
                                                 const Identifier &schema,
                                                 OnEntryNotFound if_not_found);

  optional_ptr<CatalogEntry> CreateSchema(transaction::TransactionContext &context,
                                          Identifier &name);

  // Get dependency manager
  optional_ptr<DependencyManager> GetDependencyManager();

  optional_ptr<CatalogEntry> CreateTable(transaction::TransactionContext &context,
                                         CreateTableInfo &info, SchemaCatalogEntry &schema);

  optional_ptr<CatalogEntry> CreateIndex(transaction::TransactionContext &context,
                                         CreateIndexInfo &info, TableCatalogEntry &table);
};
} // namespace db7::catalog