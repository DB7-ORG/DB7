#pragma once

#include "catalog/entries/dependency_entry.hpp"
namespace db7::catalog {

class DependencySubjectEntry : public DependencyEntry {
public:
  ~DependencySubjectEntry() override;
  DependencySubjectEntry(DatabaseCatalog &catalog, const DependencyInfo &info);

public:
  const CatalogEntryInfo &EntryInfo() const override;
  const MangledEntryName &EntryMangledName() const override;
  const CatalogEntryInfo &SourceInfo() const override;
  const MangledEntryName &SourceMangledName() const override;
};
} // namespace db7::catalog