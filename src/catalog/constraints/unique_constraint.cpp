#include "catalog/constraints//unique_constraint.hpp"
#include "shared/macro_helper.hpp"

namespace db7::catalog {

bool UniqueConstraint::IsPrimaryKey() const { return is_primary_key; }

const std::vector<Identifier> &UniqueConstraint::GetColumnNames() const {
  DB7_ASSERT(!columns.empty());
  return columns;
}

} // namespace db7::catalog