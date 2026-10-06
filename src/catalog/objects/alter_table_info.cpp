#include "catalog/objects/alter_table_info.hpp"

namespace db7::catalog {

AlterInfo::AlterInfo(AlterType type, Identifier catalog_p, Identifier schema_p, Identifier name_p,
                     OnEntryNotFound if_not_found)
    : type(type), if_not_found(if_not_found), catalog(std::move(catalog_p)),
      schema(std::move(schema_p)), name(std::move(name_p)), allow_internal(false) {}

AlterInfo::AlterInfo(AlterType type) : type(type) {}

AlterInfo::~AlterInfo() {}

AlterEntryData AlterInfo::GetAlterEntryData() const {
  AlterEntryData data;
  data.catalog = catalog;
  data.schema = schema;
  data.name = name;
  data.if_not_found = if_not_found;
  return data;
}

AlterTableInfo::AlterTableInfo(AlterTableType type)
    : AlterInfo(AlterType::ALTER_TABLE), alter_table_type(type) {}

AlterTableInfo::AlterTableInfo(AlterTableType type, AlterEntryData data)
    : AlterInfo(AlterType::ALTER_TABLE, std::move(data.catalog), std::move(data.schema),
                std::move(data.name), data.if_not_found),
      alter_table_type(type) {}
AlterTableInfo::~AlterTableInfo() {}

CatalogType AlterTableInfo::GetCatalogType() const { return CatalogType::TABLE_ENTRY; }

//===--------------------------------------------------------------------===//
// AlterForeignKeyInfo
//===--------------------------------------------------------------------===//
AlterForeignKeyInfo::AlterForeignKeyInfo()
    : AlterTableInfo(AlterTableType::FOREIGN_KEY_CONSTRAINT) {}

AlterForeignKeyInfo::AlterForeignKeyInfo(AlterEntryData data, Identifier fk_table,
                                         std::vector<Identifier> pk_columns,
                                         std::vector<Identifier> fk_columns,
                                         std::vector<PhysicalIndex> pk_keys,
                                         std::vector<PhysicalIndex> fk_keys,
                                         AlterForeignKeyType type_p)
    : AlterTableInfo(AlterTableType::FOREIGN_KEY_CONSTRAINT, std::move(data)),
      fk_table(std::move(fk_table)), pk_columns(std::move(pk_columns)),
      fk_columns(std::move(fk_columns)), pk_keys(std::move(pk_keys)), fk_keys(std::move(fk_keys)),
      type(type_p) {}
AlterForeignKeyInfo::~AlterForeignKeyInfo() {}

} // namespace db7::catalog