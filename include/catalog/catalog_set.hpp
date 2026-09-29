#pragma once

#include "catalog/catalog.hpp"
#include "catalog/catalog_entry.hpp"

#include <unordered_map>

namespace db7::catalog {

class CatalogEntryMap {

private:
  //! Mapping of identifier to catalog entry
  std::unordered_map<std::string, std::unique_ptr<CatalogEntry>> entries;

public:
  CatalogEntryMap() {}

  void AddEntry(std::unique_ptr<CatalogEntry> entry);
  void UpdateEntry(std::unique_ptr<CatalogEntry> entry);
  void DropEntry(CatalogEntry &entry);
  optional_ptr<CatalogEntry> GetEntry(const std::string &name);
};

class CatalogSet {
private:
  // mutex
  Catalog &catalog;
  CatalogEntryMap map;

  //! The generator used to generate default internal entries
  // unique_ptr<DefaultGenerator> defaults;
};

} // namespace db7::catalog