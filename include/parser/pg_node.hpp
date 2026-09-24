#pragma once
// Include LAST in any .cpp file, after all project headers.

// Pull in standard headers first. Their include guards make later includes
// no-ops, so Postgres's printf/snprintf macros can never reach them.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "third_party/libpg_query/src/postgres/include/postgres.h"

#include "third_party/libpg_query/pg_query.h"
#include "third_party/libpg_query/src/pg_query_internal.h"
#include "third_party/libpg_query/src/postgres/include/nodes/parsenodes.h"
}

// Postgres's replacements for the printf family (port.h)
#undef printf
#undef fprintf
#undef sprintf
#undef snprintf
#undef vprintf
#undef vfprintf
#undef vsprintf
#undef vsnprintf
#undef strerror
#undef strerror_r

// Short names that collide with common C++ identifiers
#undef _
#undef gettext
#undef dgettext
#undef ngettext
#undef dngettext

// Log levels from elog.h
#undef DEBUG5
#undef DEBUG4
#undef DEBUG3
#undef DEBUG2
#undef DEBUG1
#undef LOG
#undef INFO
#undef NOTICE
#undef WARNING
#undef ERROR
#undef FATAL
#undef PANIC

// Safe accessors for Postgres value nodes (see earlier)
inline int PgIntVal(const void *node) { return static_cast<const ::Integer *>(node)->ival; }
inline const char *PgStrVal(const void *node) { return static_cast<const ::String *>(node)->sval; }
inline const char *PgFloatVal(const void *node) { return static_cast<const ::Float *>(node)->fval; }
inline bool PgBoolVal(const void *node) { return static_cast<const ::Boolean *>(node)->boolval; }