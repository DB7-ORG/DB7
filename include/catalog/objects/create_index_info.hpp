#pragma once

#include "catalog/objects/create_info.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {

class CreateIndexInfo : public CreateInfo {
public:
  //! The table name of the underlying table
  Identifier table;
  //! The name of the index
  Identifier index_name;
  //! The index constraint type
  IndexConstraintType constraint_type;
  //! The column ids of the indexed table
  std::vector<idx_t> column_ids;

  CreateIndexInfo();
};
} // namespace db7::catalog