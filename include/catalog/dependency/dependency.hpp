#pragma once

#include "catalog/catalog_common.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {
class CatalogEntryInfo {
public:
  CatalogType type;
  Identifier schema;
  Identifier name;

public:
  bool operator==(const CatalogEntryInfo &other) const {
    if (other.type != type) { return false; }
    if (other.schema != schema) { return false; }
    if (other.name != name) { return false; }
    return true;
  }
};
} // namespace db7::catalog