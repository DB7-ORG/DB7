#include "catalog/constraints//unique_constraint.hpp"
#include "shared/macro_helper.hpp"

namespace db7::catalog {

UniqueConstraint::UniqueConstraint(std::vector<Identifier> columns, const bool is_primary_key)
    : Constraint(ConstraintType::UNIQUE), columns(columns), is_primary_key(is_primary_key) {}

std::unique_ptr<Constraint> UniqueConstraint::Copy() const {
  return std::make_unique<UniqueConstraint>(columns, is_primary_key);
}

bool UniqueConstraint::IsPrimaryKey() const { return is_primary_key; }

const std::vector<Identifier> &UniqueConstraint::GetColumnNames() const {
  DB7_ASSERT(!columns.empty());
  return columns;
}

} // namespace db7::catalog