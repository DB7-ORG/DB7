#pragma once

#include "catalog/catalog_common.hpp"
#include "shared/error/exception.hpp"
#include "shared/identifier.hpp"
#include <memory>
namespace db7::catalog {

enum class ForeignKeyType : uint8_t {
  FK_TYPE_PRIMARY_KEY_TABLE = 0,   // main table
  FK_TYPE_FOREIGN_KEY_TABLE = 1,   // referencing table
  FK_TYPE_SELF_REFERENCE_TABLE = 2 // self referencing table
};

struct ForeignKeyInfo {
  ForeignKeyType type;
  Identifier schema;
  //! if type is FK_TYPE_FOREIGN_KEY_TABLE, means main key table, if type is
  //! FK_TYPE_PRIMARY_KEY_TABLE, means foreign key table
  Identifier table;
  //! The set of main key table's column's index
  std::vector<PhysicalIndex> pk_keys;
  //! The set of foreign key table's column's index
  std::vector<PhysicalIndex> fk_keys;

  bool IsDeleteConstraint() const {
    return type == ForeignKeyType::FK_TYPE_PRIMARY_KEY_TABLE ||
           type == ForeignKeyType::FK_TYPE_SELF_REFERENCE_TABLE;
  }
  bool IsAppendConstraint() const {
    return type == ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE ||
           type == ForeignKeyType::FK_TYPE_SELF_REFERENCE_TABLE;
  }
};

enum class ConstraintType : u8 {
  INVALID = 0,     // invalid constraint type
  NOT_NULL = 1,    // NOT NULL constraint
  CHECK = 2,       // CHECK constraint
  UNIQUE = 3,      // UNIQUE constraint
  FOREIGN_KEY = 4, // FOREIGN KEY constraint
};

class Constraint {
public:
  ConstraintType type;

  explicit Constraint(ConstraintType type) : type(type) {}
  virtual ~Constraint() {};

  virtual std::unique_ptr<Constraint> Copy() const = 0;

  template <class TARGET>
  TARGET &Cast() {
    if (type != TARGET::TYPE) {
      throw CATALOG_EXCEPTION("Failed to cast constraint to type - constraint type mismatch");
    }
    return reinterpret_cast<TARGET &>(*this);
  }

  template <class TARGET>
  const TARGET &Cast() const {
    if (type != TARGET::TYPE) {
      throw CATALOG_EXCEPTION("Failed to cast constraint to type - constraint type mismatch");
    }
    return reinterpret_cast<const TARGET &>(*this);
  }
};
} // namespace db7::catalog