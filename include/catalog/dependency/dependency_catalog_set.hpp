

#include "catalog/catalog_set.hpp"
#include "catalog/dependency/dependency_manager.hpp"

namespace db7::catalog {

//! This class mocks the CatalogSet interface, but does not actually store CatalogEntries
class DependencyCatalogSet {
public:
  CatalogSet &set;
  CatalogEntryInfo info;
  MangledEntryName mangled_name;

private:
  MangledDependencyName ApplyPrefix(const MangledEntryName &name) const;

public:
  DependencyCatalogSet(CatalogSet &set, const CatalogEntryInfo &info) : set(set), info(info), mangled_name(DependencyManager::MangleName(info)) {}

public:
  bool CreateEntry(transaction::TransactionContext &context, const MangledEntryName &name, std::unique_ptr<CatalogEntry> value);
  CatalogSet::EntryLookup GetEntryDetailed(transaction::TransactionContext &context, const MangledEntryName &name);
  optional_ptr<CatalogEntry> GetEntry(transaction::TransactionContext &context, const MangledEntryName &name);
  void Scan(transaction::TransactionContext &context, const std::function<void(CatalogEntry &)> &callback);
  bool DropEntry(transaction::TransactionContext &context, const MangledEntryName &name, bool cascade, bool allow_drop_internal = false);
};
} // namespace db7::catalog