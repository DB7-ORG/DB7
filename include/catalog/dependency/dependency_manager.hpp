#pragma once

#include "catalog/catalog_entry_helper.hpp"
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
  /**
   * Scans subjects/dependants based on scan_subjects, invoking a callback for all of them w
   * the matching prefix.
   * example: {info mangled}\0{other dep}
   * in this example callback is invoked on {other dep} if {info mangled} matches info
   * @param info          the entry whose edges are scanned (the key prefix)
   * @param scan_subjects true to scan subjects, false to scan dependents
   * @param callback      invoked once per matching edge
   */
  void ScanSetInternal(transaction::TransactionContext &context, const CatalogEntryInfo &info,
                       bool scan_subjects, dependency_callback_t &callback);
  bool IsSystemEntry(CatalogEntry &entry) const;

  void CreateDependent(transaction::TransactionContext &context, const DependencyInfo &info);
  void CreateSubject(transaction::TransactionContext &context, const DependencyInfo &info);
  void CreateDependency(transaction::TransactionContext &context, DependencyInfo &info);
  void CreateDependencies(transaction::TransactionContext &context, const CatalogEntry &object,
                          const LogicalDependencyList &dependencies);
  std::string CollectDependents(transaction::TransactionContext &context,
                                catalog_entry_set_t &entries, CatalogEntryInfo &info);
  catalog_entry_set_t CheckDropDependencies(transaction::TransactionContext &context,
                                            CatalogEntry &object, bool cascade);
  void RemoveDependency(transaction::TransactionContext &context, const DependencyInfo &info);
  void CleanupDependencies(transaction::TransactionContext &context, CatalogEntry &object);

public:
  explicit DependencyManager(DatabaseCatalog &catalog);

  CatalogSet &Dependents();
  CatalogSet &Subjects();

  // Converts CatalogEntryInfo to string format
  static MangledEntryName MangleName(const CatalogEntryInfo &info);

  // Propagates upwards looking for schema entry and returning its name
  // since every entry is either schema or child node of schema entry
  Identifier GetSchema(const CatalogEntry &entry);

  // Retrieves CatalogEntryInfo for any entry type
  // if its a dependency contains all the data to build CatalogEntryInfo
  // if its something else it propagates to schema to get the name
  CatalogEntryInfo GetLookupProperties(const CatalogEntry &entry);

  /**
   * Resolves a dependency edge to the catalog entry on its other end.
   *
   * A dependency entry stores only the identity ({type, schema, name}) of the
   * object it points to, not a pointer. Its key is
   * '{scanned object}\0{other object}', and this function looks up the
   * {other object} part, i.e. the one returned by EntryInfo():
   *   - for an entry from the dependents set: the dependent
   *     (key 'table\0main\0orders\0view\0main\0big_orders' -> view big_orders)
   *   - for an entry from the subjects set: the subject
   *     (key 'view\0main\0big_orders\0table\0main\0orders' -> table orders)
   *
   * If `dependency` is not a DEPENDENCY_ENTRY, it is already a real catalog
   * entry and is returned unchanged. If the referenced object is a schema,
   * the schema entry itself is returned.
   * @param dependency dependency entry (can be subject or dependant)
   */
  optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context,
                                         CatalogEntry &dependency);
  optional_ptr<CatalogEntry> LookupEntry(transaction::TransactionContext &context,
                                         const CatalogEntryInfo &info);

  // Wrapper around scan internal
  void ScanDependents(transaction::TransactionContext &context, const CatalogEntryInfo &info,
                      dependency_callback_t &callback);
  // Wrapper around scan internal
  void ScanSubjects(transaction::TransactionContext &context, const CatalogEntryInfo &info,
                    dependency_callback_t &callback);
  // Walks every edge in the graph
  void Scan(transaction::TransactionContext &context,
            const std::function<void(CatalogEntry &, CatalogEntry &,
                                     const DependencyDependentFlags &)> &callback);

  void AddObject(transaction::TransactionContext &context, CatalogEntry &object,
                 const LogicalDependencyList &dependencies);
  void DropObject(transaction::TransactionContext &context, CatalogEntry &object, bool cascade);
  void AlterObject(transaction::TransactionContext &context, CatalogEntry &old_obj,
                   CatalogEntry &new_obj, AlterInfo &info);
  ;
};

} // namespace db7::catalog