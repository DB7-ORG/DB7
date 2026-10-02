#include "catalog/dependency/dependency_catalog_set.hpp"
#include "catalog/catalog_set.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "catalog/entries/dependency_entry.hpp"

namespace db7::catalog {

MangledDependencyName DependencyCatalogSet::ApplyPrefix(const MangledEntryName &name) const { return MangledDependencyName(mangled_name, name); }

bool DependencyCatalogSet::CreateEntry(transaction::TransactionContext &context, const MangledEntryName &name, std::unique_ptr<CatalogEntry> value) {
  auto new_name = ApplyPrefix(name);
  const LogicalDependencyList EMPTY_DEPENDENCIES;
  return set.CreateEntry(context, new_name.name, std::move(value), EMPTY_DEPENDENCIES);
}

CatalogSet::EntryLookup DependencyCatalogSet::GetEntryDetailed(transaction::TransactionContext &context, const MangledEntryName &name) {
  auto new_name = ApplyPrefix(name);
  return set.GetEntryDetailed(context, new_name.name);
}

optional_ptr<CatalogEntry> DependencyCatalogSet::GetEntry(transaction::TransactionContext &context, const MangledEntryName &name) {
  auto new_name = ApplyPrefix(name);
  return set.GetEntry(context, new_name.name);
}

void DependencyCatalogSet::Scan(transaction::TransactionContext &context, const std::function<void(CatalogEntry &)> &callback) {
  set.ScanWithPrefix(
      context,
      [&](CatalogEntry &entry) {
        auto &dep = entry.Cast<DependencyEntry>();
        auto &from = dep.SourceMangledName();
        if (from.name != mangled_name.name) { return; }
        callback(entry);
      },
      mangled_name.name);
}

bool DependencyCatalogSet::DropEntry(transaction::TransactionContext &context, const MangledEntryName &name, bool cascade, bool allow_drop_internal) {
  auto new_name = ApplyPrefix(name);
  return set.DropEntry(context, new_name.name, cascade, allow_drop_internal);
}

} // namespace db7::catalog
