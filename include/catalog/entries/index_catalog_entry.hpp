#pragma once

#include "catalog/entries/index_catalog_entry_base.hpp"

namespace db7::catalog {

class TableCatalogEntry;
class SchemaCatalogEntry;

class IndexCatalogEntry : public IndexCatalogEntryBase {
public:
  IndexCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntry &schema,
                    CreateIndexInfo &create_info, TableCatalogEntry &table);
};
} // namespace db7::catalog