#include "catalog/objects/create_table_info.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/entries/schema_catalog_entry_base.hpp"

namespace db7::catalog {
CreateTableInfo::CreateTableInfo()
    : CreateInfo(CatalogType::TABLE_ENTRY, Identifier::InvalidSchema()) {}

CreateTableInfo::CreateTableInfo(Identifier catalog_p, Identifier schema_p, Identifier name_p)
    : CreateInfo(CatalogType::TABLE_ENTRY, std::move(schema_p), std::move(catalog_p)),
      table(std::move(name_p)) {}

CreateTableInfo::CreateTableInfo(SchemaCatalogEntryBase &schema, Identifier name_p)
    : CreateTableInfo(schema.catalog.GetName(), schema.name, std::move(name_p)) {}
} // namespace db7::catalog