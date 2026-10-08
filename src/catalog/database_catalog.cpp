#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
#include "catalog/objects/create_index_info.hpp"

namespace db7::catalog {

DatabaseCatalog::DatabaseCatalog(Identifier name)
    : name_(name), dependency_manager_(std::make_unique<DependencyManager>(*this)),
      schemas_(std::make_unique<CatalogSet>(*this)) {}

DatabaseCatalog::~DatabaseCatalog() = default;

optional_ptr<SchemaCatalogEntryBase>
DatabaseCatalog::LookupSchema(transaction::TransactionContext &context,
                              const EntryLookupInfo &schema_lookup, OnEntryNotFound if_not_found) {
  auto &schema_name = schema_lookup.GetEntryName();
  DB7_ASSERT(!schema_name.empty(), "name should be valid");
  auto entry = schemas_->GetEntry(context, Identifier(schema_name));
  if (!entry) {
    if (if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
      throw CATALOG_EXCEPTION(fmt::format("Schema with name {} does not exist!", schema_name));
    }
    return nullptr;
  }
  return &entry->Cast<SchemaCatalogEntryBase>();
}

optional_ptr<SchemaCatalogEntryBase>
DatabaseCatalog::GetSchema(transaction::TransactionContext &context, const Identifier &schema,
                           OnEntryNotFound if_not_found) {
  EntryLookupInfo schema_lookup(CatalogType::SCHEMA_ENTRY, schema);
  return LookupSchema(context, schema_lookup, if_not_found);
}

//===--------------------------------------------------------------------===//
// Schema
//===--------------------------------------------------------------------===//
optional_ptr<CatalogEntry>
DatabaseCatalog::CreateSchemaInternal(transaction::TransactionContext &context, Identifier &name) {
  LogicalDependencyList dependencies;
  auto entry = std::make_unique<SchemaCatalogEntry>(*this, name);
  auto result = entry.get();
  if (!schemas_->CreateEntry(context, name, std::move(entry), dependencies)) { return nullptr; }
  return result;
}

optional_ptr<CatalogEntry> DatabaseCatalog::CreateSchema(transaction::TransactionContext &context,
                                                         Identifier &name) {
  auto result = CreateSchemaInternal(context, name);
  if (!result) {
    throw CATALOG_EXCEPTION(
        fmt::format("Schema with name {} already exists!", name.GetIdentifierName()));
  }
  return result;
}

optional_ptr<DependencyManager> DatabaseCatalog::GetDependencyManager() {
  return dependency_manager_.get();
}

optional_ptr<CatalogEntry> DatabaseCatalog::CreateTable(transaction::TransactionContext &context,
                                                        CreateTableInfo &info,
                                                        SchemaCatalogEntry &schema) {
  return schema.CreateTable(context, info);
}

optional_ptr<CatalogEntry> DatabaseCatalog::CreateIndex(transaction::TransactionContext &context,
                                                        CreateIndexInfo &info,
                                                        TableCatalogEntry &table) {
  return table.schema.CreateIndex(context, info, table);
}

} // namespace db7::catalog
