#include "catalog/builder.hpp"
#include "access/index/btree.hpp"
#include "access/schema.hpp"
#include "access/table.hpp"
#include "catalog/catalog.hpp"
#include "catalog/catalog_common.hpp"
#include "catalog/database_catalog.hpp"
#include "shared/macro_helper.hpp"

namespace db7::catalog {

access::Schema Builder::CreateDatabaseSchema() {
  //"datoid", "datname"

  std::vector<access::SchemaColumn> columns;
  columns.reserve(2);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::DATOID), type_id::INTEGER, "datoid");

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::DATNAME), type_id::VARCHAR, "datname");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateNamespaceSchema() {
  //"nspoid", "nspname"

  std::vector<access::SchemaColumn> columns;
  columns.reserve(2);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::NSPOID), type_id::INTEGER, "nspoid");

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::NSPNAME), type_id::VARCHAR, "nspname");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateClassSchema() {
  // pg_class: tables, indexes, views, etc.
  std::vector<access::SchemaColumn> columns;
  columns.reserve(5);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELOID), type_id::INTEGER, "reloid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELNAME), type_id::VARCHAR, "relname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELNAMESPACE), type_id::INTEGER,
                       "relnamespace");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELKIND), type_id::TINYINT, "relkind");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::RELOPTIONS), type_id::VARCHAR,
                       "reloptions");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateAttributeSchema() {
  // pg_attribute: columns of all tables
  std::vector<access::SchemaColumn> columns;
  columns.reserve(7);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNUM), type_id::INTEGER, "attnum");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTRELID), type_id::INTEGER,
                       "attrelid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNAME), type_id::VARCHAR, "attname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTTYPID), type_id::INTEGER,
                       "atttypid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTLEN), type_id::SMALLINT, "attlen");
  // columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTTYPMOD),
  // type_id::INTEGER, "atttypmod");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::ATTNOTNULL), type_id::BOOLEAN,
                       "attnotnull");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateIndexSchema() {
  // pg_index: index metadata
  std::vector<access::SchemaColumn> columns;
  columns.reserve(10);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDOID), type_id::INTEGER, "indoid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDRELID), type_id::INTEGER,
                       "indrelid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISUNIQUE), type_id::BOOLEAN,
                       "indisunique");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISPRIMARY), type_id::BOOLEAN,
                       "indisprimary");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISEXCLUSION), type_id::BOOLEAN,
                       "indisexclusion");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDIMMEDIATE), type_id::BOOLEAN,
                       "indimmediate");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISVALID), type_id::BOOLEAN,
                       "indisvalid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISREADY), type_id::BOOLEAN,
                       "indisready");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::INDISLIVE), type_id::BOOLEAN,
                       "indislive");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::IND_TYPE), type_id::TINYINT,
                       "ind_type");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateTypeSchema() {
  // pg_type: data types
  std::vector<access::SchemaColumn> columns;
  columns.reserve(6);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPOID), type_id::INTEGER, "typoid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPNAME), type_id::VARCHAR, "typname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPNAMESPACE), type_id::INTEGER,
                       "typnamespace");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPLEN), type_id::SMALLINT, "typlen");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPBYVAL), type_id::BOOLEAN,
                       "typbyval");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::TYPTYPE), type_id::TINYINT, "typtype");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateConstraintSchema() {
  // pg_constraint: constraints
  std::vector<access::SchemaColumn> columns;
  columns.reserve(11);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONOID), type_id::INTEGER, "conoid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONNAME), type_id::VARCHAR, "conname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONNAMESPACE), type_id::INTEGER,
                       "connamespace");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONTYPE), type_id::TINYINT, "contype");
  // TODO its called condeferrable not condeferrabl
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONDEFERRABLE), type_id::BOOLEAN,
                       "condeferrabl");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONDEFFERED), type_id::BOOLEAN,
                       "condeffered");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONVALIDATED), type_id::BOOLEAN,
                       "convalidated");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONRELID), type_id::INTEGER,
                       "conrelid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONINDID), type_id::INTEGER,
                       "conindid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::CONFRELID), type_id::INTEGER,
                       "confrelid");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateLanguageSchema() {
  // pg_language: procedural languages
  std::vector<access::SchemaColumn> columns;
  columns.reserve(7);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANOID), type_id::INTEGER, "lanoid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANNAME), type_id::VARCHAR, "lanname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANISPL), type_id::BOOLEAN, "lanispl");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANPLTRUSTED), type_id::BOOLEAN,
                       "lanpltrusted");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANPLCALLFOID), type_id::INTEGER,
                       "lanplcallfoid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANINLINE), type_id::INTEGER,
                       "laninline");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::LANVALIDATOR), type_id::INTEGER,
                       "lanvalidator");

  return access::Schema(std::move(columns));
}

access::Schema Builder::CreateProcSchema() {
  // pg_proc: functions and procedures
  std::vector<access::SchemaColumn> columns;
  columns.reserve(21);

  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROOID), type_id::INTEGER, "prooid");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONAME), type_id::VARCHAR, "proname");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONAMESPACE), type_id::INTEGER,
                       "pronamespace");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROLANG), type_id::INTEGER, "prolang");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROCOST), type_id::DOUBLE, "procost");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROROWS), type_id::DOUBLE, "prorows");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROVARIADIC), type_id::INTEGER,
                       "provariadic");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISAGG), type_id::BOOLEAN,
                       "proisagg");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISWINDOW), type_id::BOOLEAN,
                       "proiswindow");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROISSTRICT), type_id::BOOLEAN,
                       "proisstrict");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRORETSET), type_id::BOOLEAN,
                       "proretset");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROVOLATILE), type_id::TINYINT,
                       "provolatile");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONARGS), type_id::SMALLINT,
                       "pronargs");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRONARGDEFAULTS), type_id::SMALLINT,
                       "pronargdefaults");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PRORETTYPE), type_id::INTEGER,
                       "prorettype");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGTYPES), type_id::VARBINARY,
                       "proargtypes");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROALLARGTYPES), type_id::VARBINARY,
                       "proallargtypes");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGMODES), type_id::VARBINARY,
                       "proargmodes");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGDEFAULTS), type_id::VARBINARY,
                       "proargdefaults");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROARGNAMES), type_id::VARBINARY,
                       "proargnames");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROSRC), type_id::VARCHAR, "prosrc");
  columns.emplace_back(catalog::col_oid_t(CatalogColumnOid::PROCONFIG), type_id::VARBINARY,
                       "proconfig");

  return access::Schema(std::move(columns));
}

DatabaseCatalog *Builder::CreateDatabaseCatalog(storage::BufferPool *buffer_pool,
                                                storage::DiskManagerAsync *disk_mng, db_oid_t oid) {
  DB7_ASSERT(buffer_pool != nullptr, "BufferPool must be provided");

  DatabaseCatalog *dbc = new DatabaseCatalog(oid);

  using enum CatalogTableOid;

  // Tables
  dbc->namespaces_ =
      new access::Table(buffer_pool, disk_mng, CreateNamespaceSchema(), PG_NAMESPACE, PG_VARLEN);
  dbc->classes_ =
      new access::Table(buffer_pool, disk_mng, CreateClassSchema(), PG_CLASS, PG_VARLEN);
  dbc->attributes_ =
      new access::Table(buffer_pool, disk_mng, CreateAttributeSchema(), PG_ATTRIBUTE, PG_VARLEN);
  dbc->indexes_ =
      new access::Table(buffer_pool, disk_mng, CreateIndexSchema(), PG_INDEX, PG_VARLEN);
  dbc->types_ = new access::Table(buffer_pool, disk_mng, CreateTypeSchema(), PG_TYPE, PG_VARLEN);
  dbc->constraints_ =
      new access::Table(buffer_pool, disk_mng, CreateConstraintSchema(), PG_CONSTRAINT, PG_VARLEN);
  dbc->languages_ =
      new access::Table(buffer_pool, disk_mng, CreateLanguageSchema(), PG_LANGUAGE, PG_VARLEN);
  dbc->procs_ = new access::Table(buffer_pool, disk_mng, CreateProcSchema(), PG_PROC, PG_VARLEN);

  // Indexes on pg_namespace
  dbc->namespaces_index_nspoid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_NAMESPACE_NSPOID,
                                      PG_NAMESPACE, access::AttrsFor(PG_INDEX_NAMESPACE_NSPOID));
  dbc->namespaces_index_nspname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_NAMESPACE_NSPNAME,
                                      PG_NAMESPACE, access::AttrsFor(PG_INDEX_NAMESPACE_NSPNAME));

  // Indexes on pg_class
  dbc->classes_index_reloid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_CLASS_RELOID, PG_CLASS,
                                      access::AttrsFor(PG_INDEX_CLASS_RELOID));
  dbc->classes_index_relname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_CLASS_RELNAME, PG_CLASS,
                                      access::AttrsFor(PG_INDEX_CLASS_RELNAME));
  dbc->classes_index_relnamespace_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_CLASS_RELNAMESPACE, PG_CLASS,
                                      access::AttrsFor(PG_INDEX_CLASS_RELNAMESPACE));

  // Indexes on pg_attribute
  dbc->attributes_index_attnum_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_ATTRIBUTE_ATTNUM,
                                      PG_ATTRIBUTE, access::AttrsFor(PG_INDEX_ATTRIBUTE_ATTNUM));
  dbc->attributes_index_attrelid_attname_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_ATTRIBUTE_ATTRELID_ATTNAME, PG_ATTRIBUTE,
      access::AttrsFor(PG_INDEX_ATTRIBUTE_ATTRELID_ATTNAME));

  // Indexes on pg_index
  dbc->indexes_index_indoid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_INDEX_INDOID, PG_INDEX,
                                      access::AttrsFor(PG_INDEX_INDEX_INDOID));
  dbc->indexes_index_indrelid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_INDEX_INDRELID, PG_INDEX,
                                      access::AttrsFor(PG_INDEX_INDEX_INDRELID));

  // Indexes on pg_type
  dbc->types_index_typoid_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_TYPE_TYPOID, PG_TYPE, access::AttrsFor(PG_INDEX_TYPE_TYPOID));
  dbc->types_index_typname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_TYPE_TYPNAME, PG_TYPE,
                                      access::AttrsFor(PG_INDEX_TYPE_TYPNAME));
  dbc->types_index_typnamespace_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_TYPE_TYPNAMESPACE, PG_TYPE,
                                      access::AttrsFor(PG_INDEX_TYPE_TYPNAMESPACE));

  // Indexes on pg_constraint
  dbc->constraints_index_conoid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONOID,
                                      PG_CONSTRAINT, access::AttrsFor(PG_INDEX_CONSTRAINT_CONOID));
  dbc->constraints_index_conname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONNAME,
                                      PG_CONSTRAINT, access::AttrsFor(PG_INDEX_CONSTRAINT_CONNAME));
  dbc->constraints_index_connamespace_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONNAMESPACE, PG_CONSTRAINT,
      access::AttrsFor(PG_INDEX_CONSTRAINT_CONNAMESPACE));
  dbc->constraints_index_conrelid_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONRELID, PG_CONSTRAINT,
      access::AttrsFor(PG_INDEX_CONSTRAINT_CONRELID));
  dbc->constraints_index_conindid_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONINDID, PG_CONSTRAINT,
      access::AttrsFor(PG_INDEX_CONSTRAINT_CONINDID));
  dbc->constraints_index_confrelid_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_CONSTRAINT_CONFRELID, PG_CONSTRAINT,
      access::AttrsFor(PG_INDEX_CONSTRAINT_CONFRELID));

  // Indexes on pg_language
  dbc->languages_index_lanoid_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_LANGUAGE_LANOID, PG_LANGUAGE,
                                      access::AttrsFor(PG_INDEX_LANGUAGE_LANOID));
  dbc->languages_index_lanname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_LANGUAGE_LANNAME, PG_LANGUAGE,
                                      access::AttrsFor(PG_INDEX_LANGUAGE_LANNAME));

  // Indexes on pg_proc
  dbc->procs_index_prooid_ = new access::BTreeIndex<TupleId>(
      buffer_pool, disk_mng, PG_INDEX_PROC_PROOID, PG_PROC, access::AttrsFor(PG_INDEX_PROC_PROOID));
  dbc->procs_index_proname_ =
      new access::BTreeIndex<TupleId>(buffer_pool, disk_mng, PG_INDEX_PROC_PRONAME, PG_PROC,
                                      access::AttrsFor(PG_INDEX_PROC_PRONAME));

  // Layouts
  dbc->namespace_data_chunk_layout_ = new access::DataChunkLayout(*dbc->namespaces_->GetSchema());
  dbc->classes_data_chunk_layout_ = new access::DataChunkLayout(*dbc->classes_->GetSchema());
  dbc->attribute_data_chunk_layout_ = new access::DataChunkLayout(*dbc->attributes_->GetSchema());
  dbc->indexes_data_chunk_layout_ = new access::DataChunkLayout(*dbc->indexes_->GetSchema());
  dbc->constraint_data_chunk_layout_ = new access::DataChunkLayout(*dbc->constraints_->GetSchema());

  return dbc;
}
} // namespace db7::catalog