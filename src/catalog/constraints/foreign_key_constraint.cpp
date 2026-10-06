#include "catalog/constraints/foreign_key_constraint.hpp"

#include <memory>

namespace db7::catalog {

ForeignKeyConstraint::ForeignKeyConstraint(std::vector<Identifier> pk_columns,
                                           std::vector<Identifier> fk_columns, ForeignKeyInfo info)
    : Constraint(ConstraintType::FOREIGN_KEY), pk_columns(std::move(pk_columns)),
      fk_columns(std::move(fk_columns)), info(std::move(info)) {}

std::unique_ptr<Constraint> ForeignKeyConstraint::Copy() const {
  return std::make_unique<ForeignKeyConstraint>(pk_columns, fk_columns, info);
}

ForeignKeyConstraint::~ForeignKeyConstraint() {}
} // namespace db7::catalog