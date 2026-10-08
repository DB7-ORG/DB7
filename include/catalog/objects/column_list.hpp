#pragma once

#include "shared/identifier.hpp"
#include "shared/types/type_defs.hpp"

namespace db7::catalog {

class ColumnDefinition {
private:
  //! The name of the entry
  Identifier name;
  //! The type of the column
  type_id type;

public:
  ColumnDefinition(Identifier name_p, type_id type_p);

  ColumnDefinition Copy() const;

  Identifier GetName() const { return name; };

  type_id GetType() const { return type; };
};

class ColumnList {
private:
  std::vector<ColumnDefinition> columns;
  //! A map of column name to column index
  std::unordered_map<Identifier, idx_t> name_map;

public:
  ColumnList() {}

  void AddToNameMap(ColumnDefinition &col, idx_t idx);
  void AddColumn(ColumnDefinition column);
  ColumnList Copy() const;
};
} // namespace db7::catalog