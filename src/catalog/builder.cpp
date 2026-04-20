#include "catalog/builder.hpp"
#include "catalog/database_catalog.hpp"
#include "access/table.hpp"
#include "access/schema.hpp"
#include "catalog/catalog.hpp"
#include "shared/macro_helper.hpp"
#include "catalog/catalog_common.hpp"

#include <memory>

namespace db7::catalog
{
    access::Schema Builder::CreateDatabaseSchema()
    {
        //"datoid", "datname"

        std::vector<access::SchemaColumn> columns;
        columns.reserve(2);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::DATOID), access::type_id::INTEGER, "datoid");

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::DATNAME), access::type_id::VARCHAR, "datname");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateNamespaceSchema()
    {
        //"nspoid", "nspname"

        std::vector<access::SchemaColumn> columns;
        columns.reserve(2);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::NSPOID), access::type_id::INTEGER, "nspoid");

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::NSPNAME), access::type_id::VARCHAR, "nspname");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateClassSchema()
    {
        // pg_class: tables, indexes, views, etc.
        std::vector<access::SchemaColumn> columns;
        columns.reserve(5);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELOID), access::type_id::INTEGER, "reloid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELNAME), access::type_id::VARCHAR, "relname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELNAMESPACE), access::type_id::INTEGER, "relnamespace");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELKIND), access::type_id::TINYINT, "relkind");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELOPTIONS), access::type_id::VARCHAR, "reloptions");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateAttributeSchema()
    {
        // pg_attribute: columns of all tables
        std::vector<access::SchemaColumn> columns;
        columns.reserve(7);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNUM), access::type_id::INTEGER, "attnum");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTRELID), access::type_id::INTEGER, "attrelid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNAME), access::type_id::VARCHAR, "attname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTTYPID), access::type_id::INTEGER, "atttypid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTLEN), access::type_id::SMALLINT, "attlen");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTTYPMOD), access::type_id::INTEGER, "atttypmod");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNOTNULL), access::type_id::BOOLEAN, "attnotnull");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateTypeSchema()
    {
        // pg_type: data types
        std::vector<access::SchemaColumn> columns;
        columns.reserve(6);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPOID), access::type_id::INTEGER, "typoid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPNAME), access::type_id::VARCHAR, "typname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPNAMESPACE), access::type_id::INTEGER, "typnamespace");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPLEN), access::type_id::SMALLINT, "typlen");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPBYVAL), access::type_id::BOOLEAN, "typbyval");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPTYPE), access::type_id::TINYINT, "typtype");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateConstraintSchema()
    {
        // pg_constraint: constraints
        std::vector<access::SchemaColumn> columns;
        columns.reserve(11);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONOID), access::type_id::INTEGER, "conoid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONNAME), access::type_id::VARCHAR, "conname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONNAMESPACE), access::type_id::INTEGER, "connamespace");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONTYPE), access::type_id::TINYINT, "contype");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONDEFERRABLE), access::type_id::BOOLEAN, "condeferrable");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONDEFFERED), access::type_id::BOOLEAN, "condeffered");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONVALIDATED), access::type_id::BOOLEAN, "convalidated");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONRELID), access::type_id::INTEGER, "conrelid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONINDID), access::type_id::INTEGER, "conindid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONFRELID), access::type_id::INTEGER, "confrelid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONBIN), access::type_id::VARCHAR, "conbin");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateLanguageSchema()
    {
        // pg_language: procedural languages
        std::vector<access::SchemaColumn> columns;
        columns.reserve(7);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANOID), access::type_id::INTEGER, "lanoid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANNAME), access::type_id::VARCHAR, "lanname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANISPL), access::type_id::BOOLEAN, "lanispl");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANPLTRUSTED), access::type_id::BOOLEAN, "lanpltrusted");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANPLCALLFOID), access::type_id::INTEGER, "lanplcallfoid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANINLINE), access::type_id::INTEGER, "laninline");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANVALIDATOR), access::type_id::INTEGER, "lanvalidator");

        return access::Schema(columns);
    }

    access::Schema Builder::CreateProcSchema()
    {
        // pg_proc: functions and procedures
        std::vector<access::SchemaColumn> columns;
        columns.reserve(21);

        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROOID), access::type_id::INTEGER, "prooid");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONAME), access::type_id::VARCHAR, "proname");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONAMESPACE), access::type_id::INTEGER, "pronamespace");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROLANG), access::type_id::INTEGER, "prolang");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROCOST), access::type_id::DOUBLE, "procost");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROROWS), access::type_id::DOUBLE, "prorows");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROVARIADIC), access::type_id::INTEGER, "provariadic");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISAGG), access::type_id::BOOLEAN, "proisagg");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISWINDOW), access::type_id::BOOLEAN, "proiswindow");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISSTRICT), access::type_id::BOOLEAN, "proisstrict");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRORETSET), access::type_id::BOOLEAN, "proretset");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROVOLATILE), access::type_id::TINYINT, "provolatile");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONARGS), access::type_id::SMALLINT, "pronargs");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONARGDEFAULTS), access::type_id::SMALLINT, "pronargdefaults");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRORETTYPE), access::type_id::INTEGER, "prorettype");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGTYPES), access::type_id::VARBINARY, "proargtypes");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROALLARGTYPES), access::type_id::VARBINARY, "proallargtypes");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGMODES), access::type_id::VARBINARY, "proargmodes");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGDEFAULTS), access::type_id::VARBINARY, "proargdefaults");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGNAMES), access::type_id::VARBINARY, "proargnames");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROSRC), access::type_id::VARCHAR, "prosrc");
        columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROCONFIG), access::type_id::VARBINARY, "proconfig");

        return access::Schema(columns);
    }

    DatabaseCatalog *Builder::CreateDatabaseCatalog(storage::BufferPool *buffer_pool)
    {
        DB7_ASSERT(buffer_pool != nullptr, "BufferPool must be provided");

        DatabaseCatalog *dbc = new DatabaseCatalog();

        dbc->namespaces_ = new access::Table(buffer_pool, CreateNamespaceSchema(), rel_oid_t(CatalogTableOid::PG_NAMESPACE));
        dbc->classes_ = new access::Table(buffer_pool, CreateClassSchema(), rel_oid_t(CatalogTableOid::PG_CLASS));
        dbc->attributes_ = new access::Table(buffer_pool, CreateAttributeSchema(), rel_oid_t(CatalogTableOid::PG_ATTRIBUTE));
        dbc->types_ = new access::Table(buffer_pool, CreateTypeSchema(), rel_oid_t(CatalogTableOid::PG_TYPE));
        dbc->constraints_ = new access::Table(buffer_pool, CreateConstraintSchema(), rel_oid_t(CatalogTableOid::PG_CONSTRAINT));
        dbc->languages_ = new access::Table(buffer_pool, CreateLanguageSchema(), rel_oid_t(CatalogTableOid::PG_LANGUAGE));
        dbc->procs_ = new access::Table(buffer_pool, CreateProcSchema(), rel_oid_t(CatalogTableOid::PG_PROC));

        return dbc;
    }
}