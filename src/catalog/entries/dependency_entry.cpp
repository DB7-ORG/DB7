#include "catalog/entries/dependency_entry.hpp"

namespace db7::catalog {

DependencyEntry::DependencyEntry(DatabaseCatalog &catalog, DependencyEntryType side,
                                 const MangledDependencyName &name, const DependencyInfo &info)
    : InCatalogEntry(CatalogType::DEPENDENCY_ENTRY, catalog, name.name), side(side),
      dependent_name(DependencyManager::MangleName(info.dependent.entry)),
      subject_name(DependencyManager::MangleName(info.subject.entry)), dependent(info.dependent),
      subject(info.subject) {
  DB7_ASSERT(info.dependent.entry.type != CatalogType::DEPENDENCY_ENTRY, "invalid type");
  DB7_ASSERT(info.subject.entry.type != CatalogType::DEPENDENCY_ENTRY, "invalid type");
  // if (catalog.IsTemporaryCatalog()) { temporary = true; }
  // TODO catalog figure out how to handle temporary
}

const MangledEntryName &DependencyEntry::SubjectMangledName() const { return subject_name; }

const DependencySubject &DependencyEntry::Subject() const { return subject; }

const MangledEntryName &DependencyEntry::DependentMangledName() const { return dependent_name; }

const DependencyDependent &DependencyEntry::Dependent() const { return dependent; }

DependencyEntry::~DependencyEntry() {}

DependencyEntryType DependencyEntry::Side() const { return side; }

} // namespace db7::catalog
