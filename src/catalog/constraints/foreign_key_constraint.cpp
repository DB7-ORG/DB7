#include "catalog/constraints/foreign_key_constraint.hpp"
#include "shared/macro_helper.hpp"

#include <memory>

namespace db7::catalog {

ForeignKeyConstraint::ForeignKeyConstraint(std::vector<Identifier> pk_columns,
                                           std::vector<Identifier> fk_columns, ForeignKeyInfo i)
    : Constraint(ConstraintType::FOREIGN_KEY), pk_columns(std::move(pk_columns)),
      fk_columns(std::move(fk_columns)), info(std::move(i)) {
  DB7_ASSERT(info.fk_keys.size() == info.pk_keys.size(), "Should be same size");
}

std::unique_ptr<Constraint> ForeignKeyConstraint::Copy() const {
  return std::make_unique<ForeignKeyConstraint>(pk_columns, fk_columns, info);
}

ForeignKeyConstraint::~ForeignKeyConstraint() {}
} // namespace db7::catalog