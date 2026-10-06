#include "catalog/objects/create_table_info.hpp"

namespace db7::catalog {
CreateTableInfo::CreateTableInfo(SchemaCatalogEntryBase &schema, Identifier table,
                                 ColumnList columns,
                                 std::vector<std::unique_ptr<Constraint>> constraints,
                                 LogicalDependencyList dependencies)
    : schema(schema), table(std::move(table)), columns(std::move(columns)),
      constraints(std::move(constraints)), dependencies(std::move(dependencies)) {}
} // namespace db7::catalog