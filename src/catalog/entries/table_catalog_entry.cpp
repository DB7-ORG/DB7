#include "catalog/entries/table_catalog_entry.hpp"
#include "access/data_table.hpp"
#include "access/index/btree.hpp"
#include "access/objects/add_table_index_info.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/constraints/unique_constraint.hpp"
#include "storage/storage_manager.hpp"

namespace db7::catalog {
TableCatalogEntry::TableCatalogEntry(DatabaseCatalog &catalog, SchemaCatalogEntryBase &schema,
                                     CreateTableInfo &info,
                                     std::shared_ptr<access::DataTable> inherited_storage)
    : TableCatalogEntryBase(catalog, schema, info), storage(std::move(inherited_storage)) {

  if (storage) { return; }

  table_id oid = NextOid();

  std::vector<storage::StorageColumnDesc> column_defs;
  column_t column_id = 0;
  for (const auto &col : info.columns) { column_defs.push_back({column_id++, col.GetType()}); }
  storage = std::make_shared<access::DataTable>(*this, column_defs, oid);

  // create indexes for primary, unique and foreign key constraints
  auto *buffer_pool = &catalog.StorageManager().GetBufferPool();
  auto *disk = &catalog.StorageManager().GetDiskManager();
  for (idx_t i = 0; i < constraints.size(); i++) {
    auto &constraint = constraints[i];
    switch (constraint->type) {
    case ConstraintType::UNIQUE: {
      auto &unique = constraint->Cast<UniqueConstraint>();
      IndexConstraintType constraint_type =
          unique.IsPrimaryKey() ? IndexConstraintType::PRIMARY : IndexConstraintType::UNIQUE;

      std::vector<access::TypeSize> index_columns;
      for (const auto &name : unique.GetColumnNames()) {
        const auto id = info.columns.GetColumnId(name);
        const auto col = column_defs[id];
        index_columns.push_back({col.column_id, col.type});
      }

      auto btree = std::make_unique<access::BTreeIndex<>>(buffer_pool, disk, oid, 0, index_columns);
      auto info = std::make_unique<access::AddTableIndexInfo>(std::move(btree), constraint_type);
      storage->AddIndex(std::move(info));
    }

    case ConstraintType::FOREIGN_KEY: {
      auto &bfk = constraint->Cast<ForeignKeyConstraint>();
      if (bfk.info.type == ForeignKeyType::FK_TYPE_PRIMARY_KEY_TABLE) { continue; }
      auto constraint_type = IndexConstraintType::FOREIGN;

      std::vector<access::TypeSize> index_columns;
      for (const auto &name : bfk.pk_columns) {
        const auto id = info.columns.GetColumnId(name);
        const auto col = column_defs[id];
        index_columns.push_back({col.column_id, col.type});
      }

      auto btree = std::make_unique<access::BTreeIndex<>>(buffer_pool, disk, oid, 0, index_columns);
      auto info = std::make_unique<access::AddTableIndexInfo>(std::move(btree), constraint_type);
      storage->AddIndex(std::move(info));
    }
    case ConstraintType::CHECK:
    case ConstraintType::INVALID:
    case ConstraintType::NOT_NULL: break;
    }
  }
};

table_id TableCatalogEntry::NextOid() { return next_oid_.fetch_add(1, std::memory_order_relaxed); }

const std::vector<std::unique_ptr<Constraint>> &TableCatalogEntry::GetConstraints() const {
  return constraints;
}

std::unique_ptr<CatalogEntry>
TableCatalogEntry::AlterEntry(transaction::TransactionContext &context, AlterInfo &info) {
  // Always expects AlterTableInfo since this is a table
  auto &table_info = info.Cast<AlterTableInfo>();

  switch (table_info.alter_table_type) {
  case AlterTableType::FOREIGN_KEY_CONSTRAINT: {
    // In case of foreign key expects AlterForeignKeyInfo
    auto &foreign_key_constraint_info = table_info.Cast<AlterForeignKeyInfo>();
    // Branch to check to either remove or add new dependencies
    if (foreign_key_constraint_info.type == AlterForeignKeyType::AFT_ADD) {
      return AddForeignKeyConstraint(foreign_key_constraint_info);
    } else {
      // return DropForeignKeyConstraint(context, foreign_key_constraint_info);
    }
  }
  default: throw; // TODO catalog add more stuff
  }
}

// TODO catalog
// this is a litle strange since when creating a table we map to some object then
// map that object to original form
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