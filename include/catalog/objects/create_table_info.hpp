#pragma once

#include "catalog/constraints/constraint.hpp"
#include "catalog/objects/column_list.hpp"
#include "catalog/objects/create_info.hpp"

namespace db7::catalog {
class SchemaCatalogEntryBase;

class CreateTableInfo : public CreateInfo {
public:
  //! Table name to insert
  Identifier table;
  //! List of columns of the table
  ColumnList columns;
  //! List of constraints on the table
  std::vector<std::unique_ptr<Constraint>> constraints;

  CreateTableInfo();
  CreateTableInfo(SchemaCatalogEntryBase &schema, Identifier table);
  CreateTableInfo(Identifier catalog, Identifier schema, Identifier table);
};
} // namespace db7::catalog