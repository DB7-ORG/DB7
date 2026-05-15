#pragma once

#include "common.hpp"

#define INVALID_REL_OID 0

namespace db7::catalog
{
    using db_oid_t = u32;
    using rel_oid_t = u32;
    using col_oid_t = u32;

    enum class CatalogTableOid : rel_oid_t
    {
        PG_NAMESPACE = 1,
        PG_CLASS = 2,
        PG_ATTRIBUTE = 3,
        PG_TYPE = 4,
        PG_CONSTRAINT = 5,
        PG_LANGUAGE = 6,
        PG_PROC = 7,
        PG_VARLEN = 8,

        // pg_namespace indexes
        PG_INDEX_NAMESPACE_NSPOID = 9,
        PG_INDEX_NAMESPACE_NSPNAME = 10,

        // pg_class indexes
        PG_INDEX_CLASS_RELOID = 11,
        PG_INDEX_CLASS_RELNAME = 12,
        PG_INDEX_CLASS_RELNAMESPACE = 13,

        // pg_attribute indexes
        PG_INDEX_ATTRIBUTE_ATTNUM = 14,
        PG_INDEX_ATTRIBUTE_ATTRELID = 15,
        PG_INDEX_ATTRIBUTE_ATTNAME = 16,

        // pg_type indexes
        PG_INDEX_TYPE_TYPOID = 17,
        PG_INDEX_TYPE_TYPNAME = 18,
        PG_INDEX_TYPE_TYPNAMESPACE = 19,

        // pg_constraint indexes
        PG_INDEX_CONSTRAINT_CONOID = 20,
        PG_INDEX_CONSTRAINT_CONNAME = 21,
        PG_INDEX_CONSTRAINT_CONNAMESPACE = 22,
        PG_INDEX_CONSTRAINT_CONRELID = 23,
        PG_INDEX_CONSTRAINT_CONINDID = 24,
        PG_INDEX_CONSTRAINT_CONFRELID = 25,

        // pg_language indexes
        PG_INDEX_LANGUAGE_LANOID = 26,
        PG_INDEX_LANGUAGE_LANNAME = 27,

        // pg_proc indexes
        PG_INDEX_PROC_PROOID = 28,
        PG_INDEX_PROC_PRONAME = 29,

        // database index
        PG_DATABASE_DATOID = 30,
        PG_DATABASE_DATNAME = 31,
    };

    enum class CatalogColumnOid : col_oid_t
    {
        // database
        DATOID = 1,
        DATNAME,
        // namespace
        NSPOID,
        NSPNAME,
        // table
        RELOID,
        RELNAME,
        RELNAMESPACE,
        RELKIND,
        RELOPTIONS,
        // column
        ATTNUM,
        ATTRELID,
        ATTNAME,
        ATTTYPID,
        ATTLEN,
        ATTTYPMOD,
        ATTNOTNULL,
        // type
        TYPOID,
        TYPNAME,
        TYPNAMESPACE,
        TYPLEN,
        TYPBYVAL,
        TYPTYPE,
        // constraint
        CONOID,
        CONNAME,
        CONNAMESPACE,
        CONTYPE,
        CONDEFERRABLE,
        CONDEFFERED,
        CONVALIDATED,
        CONRELID,
        CONINDID,
        CONFRELID,
        CONBIN,
        // language
        LANOID,
        LANNAME,
        LANISPL,
        LANPLTRUSTED,
        LANPLCALLFOID,
        LANINLINE,
        LANVALIDATOR,
        // proc table schema
        PROOID,
        PRONAME,
        PRONAMESPACE,
        PROLANG,
        PROCOST,
        PROROWS,
        PROVARIADIC,
        PROISAGG,
        PROISWINDOW,
        PROISSTRICT,
        PRORETSET,
        PROVOLATILE,
        PRONARGS,
        PRONARGDEFAULTS,
        PRORETTYPE,
        PROARGTYPES,
        PROALLARGTYPES,
        PROARGMODES,
        PROARGDEFAULTS,
        PROARGNAMES,
        PROSRC,
        PROCONFIG,
    };
}