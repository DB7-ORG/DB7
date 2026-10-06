#pragma once

#include "catalog/catalog_common.hpp"
#include "catalog/dependency/dependency_list.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {

enum class AlterType : u8 {
  INVALID = 0,
  ALTER_TABLE = 1,
  ALTER_VIEW = 2,
  ALTER_SEQUENCE = 3,
  CHANGE_OWNERSHIP = 4,
  ALTER_SCALAR_FUNCTION = 5,
  ALTER_TABLE_FUNCTION = 6,
  SET_COMMENT = 7,
  SET_COLUMN_COMMENT = 8,
  ALTER_DATABASE = 9
};

enum class AlterBindMode { BIND_ON_ALTER, SKIP_BINDING };

struct AlterEntryData {
  AlterEntryData() {}
  AlterEntryData(Identifier catalog_p, Identifier schema_p, Identifier name_p,
                 OnEntryNotFound if_not_found)
      : catalog(std::move(catalog_p)), schema(std::move(schema_p)), name(std::move(name_p)),
        if_not_found(if_not_found) {}

  Identifier catalog;
  Identifier schema;
  Identifier name;
  OnEntryNotFound if_not_found;
};

class AlterInfo {

public:
  AlterInfo(AlterType type, Identifier catalog, Identifier schema, Identifier name,
            OnEntryNotFound if_not_found);
  virtual ~AlterInfo();

  AlterType type;
  //! if exists
  OnEntryNotFound if_not_found;
  //! Catalog name to alter
  Identifier catalog;
  //! Schema name to alter
  Identifier schema;
  //! Entry name to alter
  Identifier name;
  //! Allow altering internal entries
  bool allow_internal;
  //! Determine whether to skip Bind
  AlterBindMode bind_mode = AlterBindMode::BIND_ON_ALTER;
  //! New dependencies for the altered entry (set during binding)
  std::unique_ptr<LogicalDependencyList> new_dependencies;

public:
  virtual CatalogType GetCatalogType() const = 0;

  virtual Identifier GetColumnName() const { return Identifier(); };

  AlterEntryData GetAlterEntryData() const;

  template <class TARGET>
  TARGET &Cast() {
    return reinterpret_cast<TARGET &>(*this);
  }

  template <class TARGET>
  const TARGET &Cast() const {
    return reinterpret_cast<const TARGET &>(*this);
  }

protected:
  explicit AlterInfo(AlterType type);
};

enum class AlterTableType : uint8_t {
  INVALID = 0,
  RENAME_COLUMN = 1,
  RENAME_TABLE = 2,
  ADD_COLUMN = 3,
  REMOVE_COLUMN = 4,
  ALTER_COLUMN_TYPE = 5,
  SET_DEFAULT = 6,
  FOREIGN_KEY_CONSTRAINT = 7,
  SET_NOT_NULL = 8,
  DROP_NOT_NULL = 9,
  SET_COLUMN_COMMENT = 10,
  ADD_CONSTRAINT = 11,
  SET_PARTITIONED_BY = 12,
  SET_SORTED_BY = 13,
  ADD_FIELD = 14,
  REMOVE_FIELD = 15,
  RENAME_FIELD = 16,
  SET_TABLE_OPTIONS = 17,
  RESET_TABLE_OPTIONS = 18,
};

struct AlterTableInfo : public AlterInfo {
  AlterTableInfo(AlterTableType type, AlterEntryData data);
  ~AlterTableInfo();

  AlterTableType alter_table_type;

public:
  CatalogType GetCatalogType() const override;

protected:
  explicit AlterTableInfo(AlterTableType type);
};

enum class AlterForeignKeyType : uint8_t { AFT_ADD = 0, AFT_DELETE = 1 };

struct AlterForeignKeyInfo : public AlterTableInfo {
  AlterForeignKeyInfo(AlterEntryData data, Identifier fk_table, std::vector<Identifier> pk_columns,
                      std::vector<Identifier> fk_columns, std::vector<PhysicalIndex> pk_keys,
                      std::vector<PhysicalIndex> fk_keys, AlterForeignKeyType type);
  ~AlterForeignKeyInfo();

  Identifier fk_table;
  std::vector<Identifier> pk_columns;
  std::vector<Identifier> fk_columns;
  std::vector<PhysicalIndex> pk_keys;
  std::vector<PhysicalIndex> fk_keys;
  AlterForeignKeyType type;

private:
  AlterForeignKeyInfo();
};
} // namespace db7::catalog