#include "catalog/entries/table_catalog_entry.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"

namespace db7::catalog {
TableCatalogEntry::TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntry &schema,
                                     CreateTableInfo &info,
                                     std::shared_ptr<StorageTable> inherited_storage)
    : TableCatalogEntryBase(catalog, schema, info), storage(std::move(inherited_storage)) {};

const std::vector<std::unique_ptr<Constraint>> &TableCatalogEntry::GetConstraints() const {
  return constraints;
}
std::unique_ptr<CatalogEntry>
TableCatalogEntry::AlterEntry(transaction::TransactionContext &context, AlterInfo &info) {
  auto &table_info = info.Cast<AlterTableInfo>();
  switch (table_info.alter_table_type) {
  case AlterTableType::FOREIGN_KEY_CONSTRAINT: {
    auto &foreign_key_constraint_info = table_info.Cast<AlterForeignKeyInfo>();
    if (foreign_key_constraint_info.type == AlterForeignKeyType::AFT_ADD) {
      return AddForeignKeyConstraint(foreign_key_constraint_info);
    } else {
      // return DropForeignKeyConstraint(context, foreign_key_constraint_info);
    }
  }
  default: throw; // TODO catalog add more stuff
  }
}

std::unique_ptr<CatalogEntry>
TableCatalogEntry::AddForeignKeyConstraint(AlterForeignKeyInfo &info) {
  DB7_ASSERT(info.type == AlterForeignKeyType::AFT_ADD);
  auto create_info = std::make_unique<CreateTableInfo>(schema, name);

  create_info->columns = columns.Copy();
  for (idx_t i = 0; i < constraints.size(); i++) {
    create_info->constraints.push_back(constraints[i]->Copy());
  }
  ForeignKeyInfo fk_info;
  fk_info.type = ForeignKeyType::FK_TYPE_PRIMARY_KEY_TABLE;
  fk_info.schema = info.schema;
  fk_info.table = info.fk_table;
  fk_info.pk_keys = info.pk_keys;
  fk_info.fk_keys = info.fk_keys;
  create_info->constraints.push_back(
      std::make_unique<ForeignKeyConstraint>(info.pk_columns, info.fk_columns, std::move(fk_info)));

  return std::make_unique<TableCatalogEntry>(catalog, schema, *create_info, storage);
}

} // namespace db7::catalog