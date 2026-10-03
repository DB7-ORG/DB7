#pragma once

#include "catalog/dependency/dependency.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::catalog {
enum class DependencyEntryType : uint8_t { SUBJECT, DEPENDENT };

class DependencyEntry : public InCatalogEntry {
private:
  DependencyEntryType side;

protected:
  const MangledEntryName dependent_name;
  const MangledEntryName subject_name;
  const DependencyDependent dependent;
  const DependencySubject subject;

protected:
  DependencyEntry(DatabaseCatalog &catalog, DependencyEntryType type,
                  const MangledDependencyName &name, const DependencyInfo &info);

public:
  ~DependencyEntry() override;

public:
  virtual const CatalogEntryInfo &EntryInfo() const = 0;
  virtual const MangledEntryName &EntryMangledName() const = 0;
  virtual const CatalogEntryInfo &SourceInfo() const = 0;
  virtual const MangledEntryName &SourceMangledName() const = 0;

  const MangledEntryName &SubjectMangledName() const;
  const DependencySubject &Subject() const;

  const MangledEntryName &DependentMangledName() const;
  const DependencyDependent &Dependent() const;

  DependencyEntryType Side() const;
};
} // namespace db7::catalog