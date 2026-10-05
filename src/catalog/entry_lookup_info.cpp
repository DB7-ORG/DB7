#include "catalog/entry_lookup_info.hpp"

namespace db7::catalog {
EntryLookupInfo::EntryLookupInfo(CatalogType catalog_type_p, Identifier name_p) : catalog_type(catalog_type_p), name(std::move(name_p)) {}

EntryLookupInfo EntryLookupInfo::SchemaLookup(const EntryLookupInfo &parent, Identifier schema_name) {
  return EntryLookupInfo(CatalogType::SCHEMA_ENTRY, std::move(schema_name));
}

CatalogType EntryLookupInfo::GetCatalogType() const { return catalog_type; }

const Identifier &EntryLookupInfo::GetEntryIdentifier() const { return name; }

const std::string &EntryLookupInfo::GetEntryName() const { return name.GetIdentifierName(); }
} // namespace db7::catalog