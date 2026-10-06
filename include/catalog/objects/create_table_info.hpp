#pragma once

#include "catalog/constraints/constraint.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "catalog/objects/column_list.hpp"

namespace db7::catalog {
class SchemaCatalogEntry;

class CreateTableInfo {
public:
  //! The schema to create the table in
  SchemaCatalogEntryBase &schema;
  //! Table name to insert
  Identifier table;
  //! List of columns of the table
  ColumnList columns;
  //! List of constraints on the table
  std::vector<std::unique_ptr<Constraint>> constraints;
  //! Dependents of the table (in e.g. default values)
  LogicalDependencyList dependencies;

  CreateTableInfo(SchemaCatalogEntryBase &schema, Identifier table, ColumnList columns = {},
                  std::vector<std::unique_ptr<Constraint>> constraints = {},
                  LogicalDependencyList dependencies = {});
};
} // namespace db7::catalog