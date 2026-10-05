#pragma once

#include "catalog/catalog_common.hpp"
#include "catalog/objects/create_info.hpp"

namespace db7::catalog {
class SchemaCatalogEntry;

class BoundCreateTableInfo {
public:
  //! The schema to create the table in
  SchemaCatalogEntry &schema;
  //! The base CreateInfo object
  std::unique_ptr<CreateInfo> base;
  //! Column dependency manager of the table
  // ColumnDependencyManager column_dependency_manager;
  //! List of constraints on the table
  std::vector<std::unique_ptr<Constraint>> constraints;
  //! Dependents of the table (in e.g. default values)
  LogicalDependencyList dependencies;
  // //! The existing table data on disk (if any)
  // unique_ptr<PersistentTableData> data;
  // //! CREATE TABLE from QUERY
  // unique_ptr<LogicalOperator> query;
  // //! Indexes created by this table
  // vector<IndexStorageInfo> indexes;
};
} // namespace db7::catalog