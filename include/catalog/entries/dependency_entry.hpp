#pragma once

#include "catalog/dependency/dependency.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::catalog {
class DependencyEntry : public InCatalogEntry {
protected:
  const MangledEntryName dependent_name;
  const MangledEntryName subject_name;
  const DependencyDependent dependent;
  const DependencySubject subject;

public:
  virtual const CatalogEntryInfo &EntryInfo() const = 0;

  const DependencyDependent &Dependent() const { return dependent; }
};
} // namespace db7::catalog