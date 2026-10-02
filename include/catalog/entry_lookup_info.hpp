#pragma once

#include "catalog/catalog_common.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {
struct EntryLookupInfo {

private:
  CatalogType catalog_type;
  Identifier name;

public:
  EntryLookupInfo(CatalogType catalog_type, Identifier name);

  static EntryLookupInfo SchemaLookup(const EntryLookupInfo &parent, Identifier schema_name);

  CatalogType GetCatalogType() const;
  //! The identifier being looked up (the catalog stores/compares names case-insensitively).
  const Identifier &GetEntryIdentifier() const;
  //! The raw name of the identifier being looked up.
  const std::string &GetEntryName() const;
};
} // namespace db7::catalog