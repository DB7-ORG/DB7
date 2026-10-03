#pragma once

#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency.hpp"
#include "catalog/entries/catalog_entry.hpp"
#include "shared/identifier.hpp"

#include <unordered_set>

namespace db7::catalog {

class LogicalDependency {
public:
  CatalogEntryInfo entry;
  Identifier catalog;

public:
  explicit LogicalDependency(CatalogEntry &entry);
  LogicalDependency();
  LogicalDependency(optional_ptr<DatabaseCatalog> catalog, CatalogEntryInfo entry,
                    Identifier catalog_str);
  bool operator==(const LogicalDependency &other) const;
};

struct LogicalDependencyHashFunction {
  uint64_t operator()(const LogicalDependency &a) const;
};

struct LogicalDependencyEquality {
  bool operator()(const LogicalDependency &a, const LogicalDependency &b) const;
};

class LogicalDependencyList {
private:
  using create_info_set_t = std::unordered_set<LogicalDependency, LogicalDependencyHashFunction,
                                               LogicalDependencyEquality>;
  create_info_set_t set;

public:
  void AddDependency(CatalogEntry &entry);
  void AddDependency(const LogicalDependency &entry);
  bool Contains(CatalogEntry &entry);
  const create_info_set_t &Set() const;
};
} // namespace db7::catalog