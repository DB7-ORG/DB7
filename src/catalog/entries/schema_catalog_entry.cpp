#include "catalog/entries/schema_catalog_entry.hpp"
#include "catalog/entries/table_catalog_entry.hpp"
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

optional_ptr<CatalogEntry> CreateTable(transaction::TransactionContext &context,
                                       BoundCreateTableInfo &info) {
  // auto table = std::make_unique<TableCatalogEntry>(catalog, *this, info);
}

} // namespace db7::catalog