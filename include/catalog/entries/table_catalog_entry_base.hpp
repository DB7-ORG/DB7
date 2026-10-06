#pragma once

#include "catalog/catalog_common.hpp"
#include "catalog/entries/standard_entry.hpp"
#include "catalog/objects/column_list.hpp"
#include "catalog/objects/create_table_info.hpp"

namespace db7::catalog {

class TableCatalogEntryBase : public StandardEntry {
protected:
  //! A list of columns that are part of this table
  ColumnList columns;
  //! A list of constraints that are part of this table
  std::vector<std::unique_ptr<Constraint>> constraints;

public:
  static constexpr const CatalogType Type = CatalogType::TABLE_ENTRY;
  static constexpr const char *Name = "table";

  TableCatalogEntryBase(DatabaseCatalog &catalog, SchemaCatalogEntryBase &schema,
                        CreateTableInfo &info);
};

} // namespace db7::catalog