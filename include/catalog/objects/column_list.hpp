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
  Identifier GetName() const;

  type_id GetType() const;
};

class ColumnList {
private:
  std::vector<ColumnDefinition> columns;
  std::unordered_map<Identifier, idx_t> map;
};
} // namespace db7::catalog