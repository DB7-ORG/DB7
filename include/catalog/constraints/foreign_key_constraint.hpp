#pragma once

#include "catalog/constraints/constraint.hpp"
#include "shared/identifier.hpp"

#include <memory>
#include <vector>

namespace db7::catalog {
class ForeignKeyConstraint : public Constraint {
public:
  static constexpr const ConstraintType TYPE = ConstraintType::FOREIGN_KEY;

public:
  std::vector<Identifier> pk_columns;
  std::vector<Identifier> fk_columns;
  ForeignKeyInfo info;

  ForeignKeyConstraint(std::vector<Identifier> pk_columns, std::vector<Identifier> fk_columns,
                       ForeignKeyInfo info);
  ~ForeignKeyConstraint() override;

  std::unique_ptr<Constraint> Copy() const override;
};
} // namespace db7::catalog