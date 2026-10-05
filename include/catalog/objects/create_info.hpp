#pragma once

#include "catalog/catalog_common.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {
class CreateInfo {
public:
  //! The to-be-created catalog type
  CatalogType type;
  //! The catalog name of the entry
  Identifier catalog;
  //! The schema name of the entry
  Identifier schema;
  //! Whether or not the entry is temporary
  bool temporary;
  //! Whether or not the entry is an internal entry
  bool internal;
  //! The inherent dependencies of the created entry
  LogicalDependencyList dependencies;

  explicit CreateInfo(CatalogType type, Identifier schema = Identifier::DefaultSchema(),
                      Identifier catalog_p = Identifier::InvalidCatalog())
      : type(type), catalog(std::move(catalog_p)), schema(std::move(schema)), temporary(false),
        internal(false) {}
};
} // namespace db7::catalog