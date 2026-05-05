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