#pragma once

#include "catalog/catalog_set.hpp"
#include "catalog/dependency/dependency.hpp"

namespace db7::catalog {
class DatabaseCatalog;
class DependencyEntry;

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

struct MangledDependencyName {
public:
  MangledDependencyName(const MangledEntryName &from, const MangledEntryName &to);
  MangledDependencyName() = delete;

public:
  //! Format: MangledEntryName\0MangledEntryName
  Identifier name;
};

class DependencyManager {
private:
  DatabaseCatalog &catalog;
  CatalogSet subjects;
  CatalogSet dependents;

private:
  using dependency_callback_t = const std::function<void(DependencyEntry &)>;

private:
  void ScanSetInternal(transaction::TransactionContext &context, const CatalogEntryInfo &info, bool scan_subjects, dependency_callback_t &callback);

public:
  explicit DependencyManager(DatabaseCatalog &catalog);

  CatalogSet &Dependents();
  CatalogSet &Subjects();
  static MangledEntryName MangleName(const CatalogEntryInfo &info);

  Identifier GetSchema(const CatalogEntry &entry);
  CatalogEntryInfo GetLookupProperties(const CatalogEntry &entry);
  optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context, CatalogEntry &dependency);

  void ScanDependents(transaction::TransactionContext &context, const CatalogEntryInfo &info, dependency_callback_t &callback);
  void Scan(transaction::TransactionContext &context,
            const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)> &callback);
};

} // namespace db7::catalog