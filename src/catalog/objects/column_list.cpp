

#include "catalog/objects/column_list.hpp"
#include "shared/error/exception.hpp"

namespace db7::catalog {
ColumnDefinition::ColumnDefinition(Identifier name_p, type_id type_p)
    : name(std::move(name_p)), type(std::move(type_p)) {}

ColumnDefinition ColumnDefinition::Copy() const {
  ColumnDefinition copy(name, type);
  return copy;
}

void ColumnList::AddToNameMap(ColumnDefinition &col, idx_t idx) {

  if (map.find(col.GetName()) != map.end()) {
    throw CATALOG_EXCEPTION(
        fmt::format("Column with name {} already exists!", col.GetName().GetIdentifierName()));
  }

  map[col.GetName()] = idx;
}

void ColumnList::AddColumn(ColumnDefinition column) {
  AddToNameMap(column, columns.size());
  columns.push_back(std::move(column));
}

ColumnList ColumnList::Copy() const {
  ColumnList result;
  for (auto &col : columns) { result.AddColumn(col.Copy()); }
  return result;
}
} // namespace db7::catalog