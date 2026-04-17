#include "catalog/builder.hpp"
#include "catalog/database_catalog.hpp"
#include "access/table.hpp"
#include "access/schema.hpp"
#include "catalog/catalog.hpp"
#include "shared/macro_helper.hpp"

#include <memory>

namespace db7::catalog
{
    access::Schema Builder::CreateDatabaseSchema()
    {
        //"datoid", "datname"

        std::vector<access::SchemaColumn> columns;
        columns.reserve(2);

        columns.emplace_back(catalog::col_oid_t(1), access::type_id::INTEGER, "datoid");

        columns.emplace_back(catalog::col_oid_t(2), access::type_id::VARCHAR, "datname");

        return access::Schema(columns);
    }

    access::Schema CreateNamespaceSchema()
    {
        //"nspoid", "nspname"

        std::vector<access::SchemaColumn> columns;
        columns.reserve(2);

        columns.emplace_back(catalog::col_oid_t(1), access::type_id::INTEGER, "nspoid");

        columns.emplace_back(catalog::col_oid_t(2), access::type_id::VARCHAR, "nspname");

        return access::Schema(columns);
    }

    DatabaseCatalog *Builder::CreateDatabaseCatalog(storage::BufferPool *buffer_pool)
    {
        DB7_ASSERT(buffer_pool != nullptr, "BufferPool must be provided");

        DatabaseCatalog *dbc = new DatabaseCatalog();

        dbc->namespaces_ = new access::Table(buffer_pool, CreateNamespaceSchema(), rel_oid_t(1));

        return dbc;
    }
}