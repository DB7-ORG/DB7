#pragma once

#include "catalog/catalog_set.hpp"

namespace db7 {
namespace catalog {
class Catalog;

class DependencyManager {
private:
  Catalog &catalog;
  CatalogSet subjects;
  CatalogSet dependents;

public:
  explicit DependencyManager(Catalog &catalog);
};

} // namespace catalog
} // namespace db7