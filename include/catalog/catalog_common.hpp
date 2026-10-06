#pragma once

#include "common.hpp"

#include <span>
#include <string>

#define INVALID_REL_OID 0

namespace db7 {
namespace access {
class Table;
}
namespace catalog {

using StorageTable = access::Table;

constexpr u32 INVALID_OID = 0;

// TODO catalog do i need all of these seems stupid
using db_oid_t = u32;
using rel_oid_t = u32;
using col_oid_t = u32;
using namespace_oid_t = u32;
using class_oid_t = u32;
using attribute_oid_t = u32;
using attribute_type_oid_t = u32;
using index_oid_t = u32;
using constraint_oid_t = u32;
using proc_oid_t = u32;

struct CatalogTableColCount {
  static constexpr u32 DATABASE = 2;  // DATOID, DATNAME
  static constexpr u32 NAMESPACE = 2; // NSPOID, NSPNAME
  static constexpr u32 CLASS = 5;     // RELOID, RELNAME, RELNAMESPACE, RELKIND, RELOPTIONS
  static constexpr u32 ATTRIBUTE =
      7; // ATTNUM, ATTRELID, ATTNAME, ATTTYPID, ATTLEN, ATTTYPMOD, ATTNOTNULL
  static constexpr u32 TYPE = 6;        // TYPOID, TYPNAME, TYPNAMESPACE, TYPLEN, TYPBYVAL, TYPTYPE
  static constexpr u32 CONSTRAINT = 12; // CONOID..CONBIN
  static constexpr u32 LANGUAGE = 7;    // LANOID..LANVALIDATOR
  static constexpr u32 PROC = 22;       // PROOID..PROCONFIG
};

enum class CatalogType : u8 {
  INVALID = 0,
  TABLE_ENTRY = 1,
  SCHEMA_ENTRY = 2,
  VIEW_ENTRY = 3,
  INDEX_ENTRY = 4,
  PREPARED_STATEMENT = 5,
  SEQUENCE_ENTRY = 6,
  COLLATION_ENTRY = 7,
  TYPE_ENTRY = 8,
  DATABASE_ENTRY = 9,
  COORDINATE_SYSTEM_ENTRY = 10,
  TRIGGER_ENTRY = 11,

  // functions
  TABLE_FUNCTION_ENTRY = 25,
  SCALAR_FUNCTION_ENTRY = 26,
  AGGREGATE_FUNCTION_ENTRY = 27,
  PRAGMA_FUNCTION_ENTRY = 28,
  COPY_FUNCTION_ENTRY = 29,
  MACRO_ENTRY = 30,
  TABLE_MACRO_ENTRY = 31,
  WINDOW_FUNCTION_ENTRY = 32,

  // version info
  DELETED_ENTRY = 51,
  RENAMED_ENTRY = 52,

  // secrets
  SECRET_ENTRY = 71,
  SECRET_TYPE_ENTRY = 72,
  SECRET_FUNCTION_ENTRY = 73,

  // dependency info
  DEPENDENCY_ENTRY = 100

};

CatalogType CatalogTypeFromString(const std::string &type);
std::string CatalogTypeToString(CatalogType type);

enum CatalogTableOid : rel_oid_t {
  PG_DATABASES = 1,
  PG_NAMESPACE,
  PG_CLASS,
  PG_INDEX,
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

  // pg_index indexes
  PG_INDEX_INDEX_INDOID,
  PG_INDEX_INDEX_INDRELID,

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

enum CatalogColumnOid : col_oid_t {
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
  // index
  INDOID,
  INDRELID,
  INDISUNIQUE,
  INDISPRIMARY,
  INDISEXCLUSION,
  INDIMMEDIATE,
  INDISVALID,
  INDISREADY,
  INDISLIVE,
  IND_TYPE,
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

enum class RelKind : char {
  REGULAR_TABLE = 'r',     ///< Ordinary table.
  INDEX = 'i',             ///< Index.
  SEQUENCE = 'S',          ///< Sequence.
  VIEW = 'v',              ///< View.
  MATERIALIZED_VIEW = 'm', ///< Materialized view.
  COMPOSITE_TYPE = 'c',    ///< Composite type.
  TOAST_TABLE = 't',       ///< TOAST table.
  FOREIGN_TABLE = 'f',     ///< Foreign table.
};

enum class IndexKind : u8 { BTREE = 0 };

enum class ConType : char {
  CHECK = 'c',              ///< Check constraint.
  FOREIGN_KEY = 'f',        ///< Foreign key constraint.
  NOT_NULL = 'n',           ///< Not-null constraint (PostgreSQL 18+).
  PRIMARY_KEY = 'p',        ///< Primary key constraint.
  UNIQUE = 'u',             ///< Unique constraint.
  CONSTRAINT_TRIGGER = 't', ///< Constraint trigger.
  EXCLUSION = 'x',          ///< Exclusion constraint.
};

template <typename T>
constexpr char ToChar(T kind) {
  return static_cast<char>(kind);
}

struct ConstraintProps {
  const std::span<byte> name;
  namespace_oid_t ns_oid;
  ConType con_type;
  bool defferable;
  bool deffered;
  bool validated;
  class_oid_t rel_oid;
  class_oid_t ind_oid;
  class_oid_t for_oid; // foreign table id
};

#define DEFAULT_SCHEMA "main"
#define INVALID_SCHEMA ""
#define INVALID_CATALOG ""
#define SYSTEM_CATALOG "system"
#define TEMP_CATALOG "temp"
#define IN_MEMORY_PATH ":memory:"

enum class OnEntryNotFound : u8 { THROW_EXCEPTION = 0, RETURN_NULL = 1 };

struct PhysicalIndex {
  static constexpr const idx_t INVALID_INDEX = idx_t(-1);

  explicit PhysicalIndex(idx_t index) : index(index) {}

  idx_t index;

  inline bool operator==(const PhysicalIndex &rhs) const { return index == rhs.index; };
  inline bool operator!=(const PhysicalIndex &rhs) const { return index != rhs.index; };
  inline bool operator<(const PhysicalIndex &rhs) const { return index < rhs.index; };
  bool IsValid() const { return index != INVALID_INDEX; }
};

} // namespace catalog
} // namespace db7