#pragma once

#include "catalog/dependency/dependency.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::catalog {
class DependencyEntry : public InCatalogEntry {
public:
  virtual const CatalogEntryInfo &EntryInfo() const = 0;
};
} // namespace db7::catalog