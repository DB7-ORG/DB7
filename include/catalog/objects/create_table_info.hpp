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
  //! Table name to insert to
  Identifier table;
  //! List of columns of the table
  ColumnList columns;
  //! List of constraints on the table
  std::vector<std::unique_ptr<Constraint>> constraints;
  //! Dependents of the table (in e.g. default values)
  LogicalDependencyList dependencies;

  CreateTableInfo(SchemaCatalogEntryBase &schema, Identifier table, ColumnList columns = {},
                  std::vector<std::unique_ptr<Constraint>> constraints = {},
                  LogicalDependencyList dependencies = {})
      : schema(schema), table(std::move(table)), columns(std::move(columns)),
        constraints(std::move(constraints)), dependencies(std::move(dependencies)) {}

  // //! The existing table data on disk (if any)
  // unique_ptr<PersistentTableData> data;
  // //! CREATE TABLE from QUERY
  // unique_ptr<LogicalOperator> query;
  // //! Indexes created by this table
  // vector<IndexStorageInfo> indexes;
  //! Column dependency manager of the table
  // ColumnDependencyManager column_dependency_manager;
};
} // namespace db7::catalog