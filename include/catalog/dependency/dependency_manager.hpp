#pragma once

#include "catalog/catalog_set.hpp"
#include "catalog/dependency/dependency.hpp"
#include "catalog/entries/dependency_entry.hpp"

namespace db7::catalog {
class DatabaseCatalog;

struct DependencySubject {
  CatalogEntryInfo entry;
  //! The type of dependency this is (e.g, ownership)
  DependencySubjectFlags flags;
};

// The entry that relies on the other entry
struct DependencyDependent {
  CatalogEntryInfo entry;
  //! The type of dependency this is (e.g, blocking, non-blocking, ownership)
  DependencyDependentFlags flags;
};

//! Every dependency consists of a subject (the entry being depended on) and a dependent (the entry
//! that has the dependency)
struct DependencyInfo {
public:
  DependencyDependent dependent;
  DependencySubject subject;

public:
  static DependencyInfo FromSubject(DependencyEntry &dep);
  static DependencyInfo FromDependent(DependencyEntry &dep);
};

struct MangledEntryName {
public:
  //! Format: Type\0Schema\0Name
  Identifier name;

public:
  explicit MangledEntryName(const CatalogEntryInfo &info);
  MangledEntryName() = delete;

public:
  bool operator==(const MangledEntryName &other) const { return other.name == name; }
  bool operator!=(const MangledEntryName &other) const { return !(*this == other); }
};

class DependencyManager {
private:
  DatabaseCatalog &catalog;
  CatalogSet subjects;
  CatalogSet dependents;

public:
  explicit DependencyManager(DatabaseCatalog &catalog);

  Identifier GetSchema(const CatalogEntry &entry);
  CatalogEntryInfo GetLookupProperties(const CatalogEntry &entry);
  optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context, CatalogEntry &dependency);

  void Scan(transaction::TransactionContext &context,
            const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)> &callback);
};

} // namespace db7::catalog