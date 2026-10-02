#include "catalog/catalog_entry_helper.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/schema_catalog_entry.hpp"

namespace db7::catalog {
DependencyManager::DependencyManager(DatabaseCatalog &catalog) : catalog(catalog), subjects(catalog), dependents(catalog) {}

Identifier DependencyManager::GetSchema(const CatalogEntry &entry) {
  if (entry.type == CatalogType::SCHEMA_ENTRY) { return entry.name; }
  return entry.ParentSchema().name;
}

CatalogEntryInfo DependencyManager::GetLookupProperties(const CatalogEntry &entry) {
  if (entry.type == CatalogType::DEPENDENCY_ENTRY) {
    auto &dependency_entry = entry.Cast<DependencyEntry>();
    return dependency_entry.EntryInfo();
  } else {
    auto schema = DependencyManager::GetSchema(entry);
    auto &name = entry.name;
    auto &type = entry.type;
    return CatalogEntryInfo{type, Identifier(schema), name};
  }
}

optional_ptr<CatalogEntry> DependencyManager::LookupEntry(transaction::TransactionContext &context, CatalogEntry &dependency) {
  if (dependency.type != CatalogType::DEPENDENCY_ENTRY) { return &dependency; }

  auto info = GetLookupProperties(dependency);

  auto &type = info.type;
  auto &schema = info.schema;
  auto &name = info.name;

  // Lookup the schema
  auto schema_entry = catalog.GetSchema(context, schema, OnEntryNotFound::RETURN_NULL);
  if (type == CatalogType::SCHEMA_ENTRY || !schema_entry) {
    // This is a schema entry, perform the callback only providing the schema
    return reinterpret_cast<CatalogEntry *>(schema_entry.get());
  }
  auto entry = schema_entry->GetEntry(context, type, name);
  return entry;
}

void DependencyManager::Scan(transaction::TransactionContext &context,
                             const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)> &callback) {
  // auto transaction = catalog.GetCatalogTransaction(context);
  // lock_guard<mutex> write_lock(catalog.GetWriteLock());

  std::lock_guard<std::mutex> write_lock(catalog.GetLock());

  // // All the objects registered in the dependency manager
  catalog_entry_set_t entries;
  dependents.Scan(context, [&](CatalogEntry &set) {
    auto entry = LookupEntry(context, set);
    entries.insert(*entry);
  });

  // // For every registered entry, get the dependents
  // for (auto &entry : entries) {
  // 	auto entry_info = GetLookupProperties(entry);
  // 	// Scan all the dependents of the entry
  // 	ScanDependents(transaction, entry_info, [&](DependencyEntry &dependent) {
  // 		auto dep = LookupEntry(transaction, dependent);
  // 		if (!dep) {
  // 			return;
  // 		}
  // 		auto &dependent_entry = *dep;
  // 		callback(entry, dependent_entry, dependent.Dependent().flags);
  // 	});
  // }
}

} // namespace db7::catalog