#pragma once

#include "common.hpp"

#define INVALID_REL_OID 0

namespace db7::catalog
{
    constexpr u32 INVALID_OID = 0;
    using db_oid_t = u32;
    using rel_oid_t = u32;
    using col_oid_t = u32;
    using namespace_oid_t = u32;
    using class_oid_t = u32;
    using attribute_oid_t = u32;
    using attribute_type_oid_t = u32;

    struct CatalogTableColCount
    {
        static constexpr u32 DATABASE = 2;    // DATOID, DATNAME
        static constexpr u32 NAMESPACE = 2;   // NSPOID, NSPNAME
        static constexpr u32 CLASS = 5;       // RELOID, RELNAME, RELNAMESPACE, RELKIND, RELOPTIONS
        static constexpr u32 ATTRIBUTE = 7;   // ATTNUM, ATTRELID, ATTNAME, ATTTYPID, ATTLEN, ATTTYPMOD, ATTNOTNULL
        static constexpr u32 TYPE = 6;        // TYPOID, TYPNAME, TYPNAMESPACE, TYPLEN, TYPBYVAL, TYPTYPE
        static constexpr u32 CONSTRAINT = 12; // CONOID..CONBIN
        static constexpr u32 LANGUAGE = 7;    // LANOID..LANVALIDATOR
        static constexpr u32 PROC = 22;       // PROOID..PROCONFIG
    };

    enum CatalogTableOid : rel_oid_t
    {
        PG_DATABASES = 1,
        PG_NAMESPACE,
        PG_CLASS,
        PG_ATTRIBUTE,
        PG_TYPE,
        PG_CONSTRAINT,
        PG_LANGUAGE,
        PG_PROC,
        PG_VARLEN,

        // pg_namespace indexes
        PG_INDEX_NAMESPACE_NSPOID,
        PG_INDEX_NAMESPACE_NSPNAME,

        // pg_class indexes
        PG_INDEX_CLASS_RELOID,
        PG_INDEX_CLASS_RELNAME,
        PG_INDEX_CLASS_RELNAMESPACE,

        // pg_attribute indexes
        PG_INDEX_ATTRIBUTE_ATTNUM,
        PG_INDEX_ATTRIBUTE_ATTRELID_ATTNAME,
        // PG_INDEX_ATTRIBUTE_ATTNAME,

        // pg_type indexes
        PG_INDEX_TYPE_TYPOID,
        PG_INDEX_TYPE_TYPNAME,
        PG_INDEX_TYPE_TYPNAMESPACE,

        // pg_constraint indexes
        PG_INDEX_CONSTRAINT_CONOID,
        PG_INDEX_CONSTRAINT_CONNAME,
        PG_INDEX_CONSTRAINT_CONNAMESPACE,
        PG_INDEX_CONSTRAINT_CONRELID,
        PG_INDEX_CONSTRAINT_CONINDID,
        PG_INDEX_CONSTRAINT_CONFRELID,

        // pg_language indexes
        PG_INDEX_LANGUAGE_LANOID,
        PG_INDEX_LANGUAGE_LANNAME,

        // pg_proc indexes
        PG_INDEX_PROC_PROOID,
        PG_INDEX_PROC_PRONAME,

        // database index
        PG_INDEX_DATABASE_DATOID,
        PG_INDEX_DATABASE_DATNAME,
    };

    enum CatalogColumnOid : col_oid_t
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
        // ATTTYPMOD,
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

    enum class RelKind : char
    {
        REGULAR_TABLE = 'r',     ///< Ordinary table.
        INDEX = 'i',             ///< Index.
        SEQUENCE = 'S',          ///< Sequence.
        VIEW = 'v',              ///< View.
        MATERIALIZED_VIEW = 'm', ///< Materialized view.
        COMPOSITE_TYPE = 'c',    ///< Composite type.
        TOAST_TABLE = 't',       ///< TOAST table.
        FOREIGN_TABLE = 'f',     ///< Foreign table.
    };

    constexpr char ToChar(RelKind kind)
    {
        return static_cast<char>(kind);
    }
}