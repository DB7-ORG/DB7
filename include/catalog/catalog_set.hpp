#pragma once

#include "catalog/entries/catalog_entry.hpp"
#include "shared/identifier.hpp"

#include <unordered_map>

namespace db7::catalog {

class Catalog;

class CatalogEntryMap {

private:
  //! Mapping of identifier to catalog entry
  std::unordered_map<Identifier, std::unique_ptr<CatalogEntry>> entries;

public:
  CatalogEntryMap() {}

  void AddEntry(std::unique_ptr<CatalogEntry> entry);
  void UpdateEntry(std::unique_ptr<CatalogEntry> entry);
  void DropEntry(CatalogEntry &entry);
  optional_ptr<CatalogEntry> GetEntry(const Identifier &name);
};

class CatalogSet {
private:
  // mutex
  DatabaseCatalog &catalog;
  CatalogEntryMap map;

  //! The generator used to generate default internal entries
  // unique_ptr<DefaultGenerator> defaults;

public:
  explicit CatalogSet(DatabaseCatalog &catalog);
  ~CatalogSet();
};

} // namespace db7::catalog