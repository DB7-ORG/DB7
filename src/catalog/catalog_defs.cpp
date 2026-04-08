#include "catalog_defs.hpp"
#include "common.hpp"

namespace noisepage::catalog
{
    STRONG_TYPEDEF_BODY(col_oid_t, u32);
    STRONG_TYPEDEF_BODY(constraint_oid_t, u32);
    STRONG_TYPEDEF_BODY(db_oid_t, u32);
    STRONG_TYPEDEF_BODY(index_oid_t, u32);
    STRONG_TYPEDEF_BODY(indexkeycol_oid_t, u32);
    STRONG_TYPEDEF_BODY(namespace_oid_t, u32);
    STRONG_TYPEDEF_BODY(language_oid_t, u32);
    STRONG_TYPEDEF_BODY(proc_oid_t, u32);
    STRONG_TYPEDEF_BODY(settings_oid_t, u32);
    STRONG_TYPEDEF_BODY(table_oid_t, u32);
    STRONG_TYPEDEF_BODY(tablespace_oid_t, u32);
    STRONG_TYPEDEF_BODY(trigger_oid_t, u32);
    STRONG_TYPEDEF_BODY(type_oid_t, u32);
    STRONG_TYPEDEF_BODY(view_oid_t, u32);
}