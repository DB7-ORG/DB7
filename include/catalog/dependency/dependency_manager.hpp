#pragma once

#include "catalog/catalog_set.hpp"

namespace db7::catalog {
class DatabaseCatalog;

class DependencyManager {
private:
  DatabaseCatalog &catalog;
  CatalogSet subjects;
  CatalogSet dependents;

public:
  explicit DependencyManager(DatabaseCatalog &catalog);
};

} // namespace db7::catalog