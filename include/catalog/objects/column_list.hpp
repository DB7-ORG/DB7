#pragma once

#include "shared/error/exception.hpp"
#include "shared/identifier.hpp"
#include "shared/types/type_defs.hpp"
#include "storage/layouts/storage_column_desc.hpp"

namespace db7::catalog {

class ColumnDefinition {
private:
  //! The name of the entry
  Identifier name;
  //! The type of the column
  type_id type;
  //! Default value for column
  storage::DefaultValue default_val; // TODO emptry for now
  //! Is column dropped
  bool isDropped = false;

public:
  ColumnDefinition(Identifier name_p, type_id type_p);

  ColumnDefinition Copy() const;

  Identifier GetName() const { return name; };

  type_id GetType() const { return type; };

  bool IsDropped() const { return isDropped; }
};

class ColumnList {
private:
  std::vector<ColumnDefinition> columns;
  //! A map of column name to column index
  std::unordered_map<Identifier, column_t> name_map;

public:
  ColumnList() {}

  void AddToNameMap(ColumnDefinition &col, column_t idx);
  void AddColumn(ColumnDefinition column);
  ColumnList Copy() const;

  column_t GetColumnId(Identifier name) const {
    auto it = name_map.find(name);
    if (it == name_map.end()) { throw CATALOG_EXCEPTION("Column not found"); }
    return it->second;
  }

public:
  using const_iterator = std::vector<ColumnDefinition>::const_iterator;

  const_iterator begin() const { return columns.begin(); }
  const_iterator end() const { return columns.end(); }

  size_t size() const { return columns.size(); }
  const ColumnDefinition &operator[](idx_t i) const { return columns[i]; }
};
} // namespace db7::catalog