#pragma once

#include "catalog/constraints/constraint.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {
class UniqueConstraint : public Constraint {
public:
  static constexpr const ConstraintType TYPE = ConstraintType::UNIQUE;

private:
  //! The names of the columns on which this constraint is defined.
  std::vector<Identifier> columns;
  //! Whether this is a PRIMARY KEY constraint, or a UNIQUE constraint.
  bool is_primary_key;

public:
  UniqueConstraint(std::vector<Identifier> columns, const bool is_primary_key);

  std::unique_ptr<Constraint> Copy() const;

  bool IsPrimaryKey() const;

  const std::vector<Identifier> &GetColumnNames() const;
};
} // namespace db7::catalog