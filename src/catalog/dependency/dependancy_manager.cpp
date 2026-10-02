#include "catalog/catalog_entry_helper.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_catalog_set.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/dependency_entry.hpp"
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

CatalogSet &DependencyManager::Dependents() { return dependents; }

CatalogSet &DependencyManager::Subjects() { return subjects; }

MangledEntryName DependencyManager::MangleName(const CatalogEntryInfo &info) { return MangledEntryName(info); }

void DependencyManager::ScanSetInternal(transaction::TransactionContext &context, const CatalogEntryInfo &info, bool scan_subjects,
                                        dependency_callback_t &callback) {
  catalog_entry_set_t other_entries;

  auto cb = [&](CatalogEntry &other) {
    DB7_ASSERT(other.type == CatalogType::DEPENDENCY_ENTRY, "");
    auto &other_entry = other.Cast<DependencyEntry>();
#ifdef DEBUG
    auto side = other_entry.Side();
    if (scan_subjects) {
      D_ASSERT(side == DependencyEntryType::SUBJECT);
    } else {
      D_ASSERT(side == DependencyEntryType::DEPENDENT);
    }

#endif

    other_entries.insert(other_entry);
    callback(other_entry);
  };

  if (scan_subjects) {
    DependencyCatalogSet subjects(Subjects(), info);
    subjects.Scan(context, cb);
  } else {
    DependencyCatalogSet dependents(Dependents(), info);
    dependents.Scan(context, cb);
  }

#ifdef DEBUG
  // Verify some invariants
  // Every dependency should have a matching dependent in the other set
  // And vice versa
  auto mangled_name = MangleName(info);

  if (scan_subjects) {
    for (auto &entry : other_entries) {
      auto other_info = GetLookupProperties(entry);
      DependencyCatalogSet other_dependents(Dependents(), other_info);

      // Verify that the other half of the dependency also exists
      auto dependent = other_dependents.GetEntryDetailed(transaction, mangled_name);
      D_ASSERT(dependent.reason != CatalogSet::EntryLookup::FailureReason::NOT_PRESENT);
    }
  } else {
    for (auto &entry : other_entries) {
      auto other_info = GetLookupProperties(entry);
      DependencyCatalogSet other_subjects(Subjects(), other_info);

      // Verify that the other half of the dependent also exists
      auto subject = other_subjects.GetEntryDetailed(transaction, mangled_name);
      D_ASSERT(subject.reason != CatalogSet::EntryLookup::FailureReason::NOT_PRESENT);
    }
  }
#endif
}

void DependencyManager::ScanDependents(transaction::TransactionContext &context, const CatalogEntryInfo &info, dependency_callback_t &callback) {
  ScanSetInternal(context, info, false, callback);
}

void DependencyManager::Scan(transaction::TransactionContext &context,
                             const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)> &callback) {
  std::lock_guard<std::mutex> write_lock(catalog.GetLock());

  // // All the objects registered in the dependency manager
  catalog_entry_set_t entries;
  dependents.Scan(context, [&](CatalogEntry &set) {
    auto entry = LookupEntry(context, set);
    entries.insert(*entry);
  });

  // // For every registered entry, get the dependents
  for (auto &entry : entries) {
    auto entry_info = GetLookupProperties(entry);
    // Scan all the dependents of the entry
    ScanDependents(context, entry_info, [&](DependencyEntry &dependent) {
      auto dep = LookupEntry(context, dependent);
      if (!dep) { return; }
      auto &dependent_entry = *dep;
      callback(entry, dependent_entry, dependent.Dependent().flags);
    });
  }
}

} // namespace db7::catalog