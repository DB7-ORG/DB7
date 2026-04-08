#pragma once

#include "strong_typedef.hpp"
#include "common.hpp"

namespace noisepage::catalog
{

    constexpr u32 NULL_OID = 0; // error return value
    constexpr u32 START_OID = 1001;

    STRONG_TYPEDEF_HEADER(col_oid_t, u32);
    STRONG_TYPEDEF_HEADER(constraint_oid_t, u32);
    STRONG_TYPEDEF_HEADER(db_oid_t, u32);
    STRONG_TYPEDEF_HEADER(index_oid_t, u32);
    STRONG_TYPEDEF_HEADER(indexkeycol_oid_t, u32);
    STRONG_TYPEDEF_HEADER(namespace_oid_t, u32);
    STRONG_TYPEDEF_HEADER(language_oid_t, u32);
    STRONG_TYPEDEF_HEADER(proc_oid_t, u32);
    STRONG_TYPEDEF_HEADER(settings_oid_t, u32);
    STRONG_TYPEDEF_HEADER(table_oid_t, u32);
    STRONG_TYPEDEF_HEADER(tablespace_oid_t, u32);
    STRONG_TYPEDEF_HEADER(trigger_oid_t, u32);
    STRONG_TYPEDEF_HEADER(type_oid_t, u32);
    STRONG_TYPEDEF_HEADER(view_oid_t, u32);

    constexpr col_oid_t INVALID_COLUMN_OID = col_oid_t(NULL_OID);
    constexpr constraint_oid_t INVALID_CONSTRAINT_OID = constraint_oid_t(NULL_OID);
    constexpr db_oid_t INVALID_DATABASE_OID = db_oid_t(NULL_OID);
    constexpr index_oid_t INVALID_INDEX_OID = index_oid_t(NULL_OID);
    constexpr indexkeycol_oid_t INVALID_INDEXKEYCOL_OID = indexkeycol_oid_t(NULL_OID);
    constexpr namespace_oid_t INVALID_NAMESPACE_OID = namespace_oid_t(NULL_OID);
    constexpr language_oid_t INVALID_LANGUAGE_OID = language_oid_t(NULL_OID);
    constexpr proc_oid_t INVALID_PROC_OID = proc_oid_t(NULL_OID);
    constexpr table_oid_t INVALID_TABLE_OID = table_oid_t(NULL_OID);
    constexpr trigger_oid_t INVALID_TRIGGER_OID = trigger_oid_t(NULL_OID);
    constexpr type_oid_t INVALID_TYPE_OID = type_oid_t(NULL_OID);
    constexpr view_oid_t INVALID_VIEW_OID = view_oid_t(NULL_OID);

    constexpr u32 TEMP_OID_MASK = 1 << 31;

}
