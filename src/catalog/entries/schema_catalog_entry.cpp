#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/constraints/foreign_key_constraint.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
#include "catalog/objects/alter_table_info.hpp"
#include "shared/error/exception.hpp"
namespace db7::catalog {

SchemaCatalogEntry::SchemaCatalogEntry(DatabaseCatalog &catalog, Identifier &schema)
    : SchemaCatalogEntryBase(catalog, schema), tables(catalog), indexes(catalog),
      table_functions(catalog), copy_functions(catalog), pragma_functions(catalog),
      functions(catalog), sequences(catalog), collations(catalog), types(catalog),
      coordinate_systems(catalog) {}

CatalogSet &SchemaCatalogEntry::GetCatalogSet(CatalogType type) {
  switch (type) {
  case CatalogType::VIEW_ENTRY:
  case CatalogType::TABLE_ENTRY: return tables;
  case CatalogType::INDEX_ENTRY: return indexes;
  case CatalogType::TABLE_FUNCTION_ENTRY:
  case CatalogType::TABLE_MACRO_ENTRY: return table_functions;
  case CatalogType::COPY_FUNCTION_ENTRY: return copy_functions;
  case CatalogType::PRAGMA_FUNCTION_ENTRY: return pragma_functions;
  case CatalogType::AGGREGATE_FUNCTION_ENTRY:
  case CatalogType::SCALAR_FUNCTION_ENTRY:
  case CatalogType::MACRO_ENTRY:
  case CatalogType::WINDOW_FUNCTION_ENTRY: return functions;
  case CatalogType::SEQUENCE_ENTRY: return sequences;
  case CatalogType::COLLATION_ENTRY: return collations;
  case CatalogType::COORDINATE_SYSTEM_ENTRY: return coordinate_systems;
  case CatalogType::TYPE_ENTRY: return types;
  default: throw CATALOG_EXCEPTION("Unsupported catalog type in schema");
  }
}

optional_ptr<CatalogEntry> SchemaCatalogEntry::LookupEntry(transaction::TransactionContext &context,
                                                           const EntryLookupInfo &lookup_info) {
  return GetCatalogSet(lookup_info.GetCatalogType())
      .GetEntry(context, lookup_info.GetEntryIdentifier());
}

static void
FindForeignKeyInformation(TableCatalogEntry &table, AlterForeignKeyType alter_fk_type,
                          std::vector<std::unique_ptr<AlterForeignKeyInfo>> &fk_arrays) {
  auto &constraints = table.GetConstraints();
  auto &catalog = table.ParentCatalog();
  auto &name = table.name;
  for (idx_t i = 0; i < constraints.size(); i++) {
    auto &cond = constraints[i];
    if (cond->type != ConstraintType::FOREIGN_KEY) { continue; }
    auto &fk = cond->Cast<ForeignKeyConstraint>();
    if (fk.info.type == ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE) {
      AlterEntryData alter_data(catalog.GetName(), fk.info.schema, fk.info.table,
                                OnEntryNotFound::THROW_EXCEPTION);
      fk_arrays.push_back(std::make_unique<AlterForeignKeyInfo>(
          std::move(alter_data), name, fk.pk_columns, fk.fk_columns, fk.info.pk_keys,
          fk.info.fk_keys, alter_fk_type));
    } else if (fk.info.type == ForeignKeyType::FK_TYPE_PRIMARY_KEY_TABLE &&
               alter_fk_type == AlterForeignKeyType::AFT_DELETE) {
      throw CATALOG_EXCEPTION(fmt::format(
          "Could not drop the table because this table is main key table of the table {}",
          fk.info.table.GetIdentifierName()));
    }
  }
}

void SchemaCatalogEntry::Alter(transaction::TransactionContext &context, AlterInfo &info) {
  CatalogType type = info.GetCatalogType();

  auto &set = GetCatalogSet(type);
  if (info.type == AlterType::CHANGE_OWNERSHIP) {
    // TODO ownership not yet supported
    // if (!set.AlterOwnership(transaction, info.Cast<ChangeOwnershipInfo>())) {
    throw CATALOG_EXCEPTION("Couldn't change ownership!");
    // }
  } else {
    auto &name = info.name;
    if (!set.AlterEntry(context, name, info)) { throw CATALOG_EXCEPTION("Couldn't alter table!"); }
  }
}

optional_ptr<CatalogEntry>
SchemaCatalogEntry::AddEntryInternal(transaction::TransactionContext &context,
                                     std::unique_ptr<StandardEntry> entry,
                                     LogicalDependencyList dependencies) {
  auto entry_name = entry->name;
  auto entry_type = entry->type;
  auto result = entry.get();

  // first find the set for this entry
  auto &set = GetCatalogSet(entry_type);
  dependencies.AddDependency(*this);

  // now try to add the entry
  if (!set.CreateEntry(context, entry_name, std::move(entry), dependencies)) {
    // entry already exists!
    return nullptr;
  }
  return result;
}

optional_ptr<CatalogEntry> SchemaCatalogEntry::CreateTable(transaction::TransactionContext &context,
                                                           CreateTableInfo &info) {
  auto table = std::make_unique<TableCatalogEntry>(catalog, *this, info);

  std::vector<std::unique_ptr<AlterForeignKeyInfo>> fk_arrays;
  FindForeignKeyInformation(*table, AlterForeignKeyType::AFT_ADD, fk_arrays);
  for (idx_t i = 0; i < fk_arrays.size(); i++) {
    // alter primary key table
    auto &fk_info = *fk_arrays[i];
    Alter(context, fk_info);

    // make a dependency between this table and referenced table
    auto &set = GetCatalogSet(CatalogType::TABLE_ENTRY);
    info.dependencies.AddDependency(*set.GetEntry(context, fk_info.name));
  }
  for (auto &dep : info.dependencies.Set()) { table->dependencies.AddDependency(dep); }

  auto entry = AddEntryInternal(context, std::move(table), info.dependencies);
  if (!entry) { return nullptr; }

  return entry;
}

} // namespace db7::catalog