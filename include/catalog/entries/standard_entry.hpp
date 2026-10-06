#pragma once

#include "catalog/dependency/dependency_list.hpp"
#include "catalog/entries/catalog_entry.hpp"

namespace db7::catalog {

class SchemaCatalogEntry;

class StandardEntry : public InCatalogEntry {
public:
  //! The schema the entry belongs to
  SchemaCatalogEntryBase &schema;
  //! The dependencies of the entry, can be empty
  LogicalDependencyList dependencies;

  StandardEntry(CatalogType type, SchemaCatalogEntryBase &schema, DatabaseCatalog &catalog,
                Identifier name)
      : InCatalogEntry(type, catalog, std::move(name)), schema(schema) {}
  ~StandardEntry() override {}

  SchemaCatalogEntryBase &ParentSchema() override { return schema; }
  const SchemaCatalogEntryBase &ParentSchema() const override { return schema; }
};
} // namespace db7::catalog