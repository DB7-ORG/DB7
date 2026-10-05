#pragma once

#include "access/index/btree.hpp"
#include "access/index_schema.hpp"
#include "access/table.hpp"
#include "catalog/builder.hpp"
#include "catalog/entries/catalog_entry.hpp"
#include "catalog/entry_lookup_info.hpp"
#include "shared/identifier.hpp"
#include "shared/models/result_object.hpp"
#include "shared/models/tuple_id.hpp"

#include <atomic>
#include <memory>

namespace db7::catalog {

class Builder;
class DependencyManager;

/**
 * Database catalog is a component managed by db7::catalog::Catalog.
 * Catalog isnt managing this component because the database lifetime is
 * strongly tied to DatabaseCatalog lifetime. Meaning this object exists as long
 * as the database. This component stores cached catalog entries. They can be
 * evicetd only using dll modifications when cache is invalidated.
 */
class DatabaseCatalog {
private:
  friend class Builder;
  // NEW:
  std::mutex mu;
  Identifier name_;
  std::unique_ptr<DependencyManager> dependency_manager_;
  std::unique_ptr<CatalogSet> schemas_;
  std::atomic<idx_t> gen_oid_;

  // OLD:
  catalog::db_oid_t db_id_;

  std::atomic<namespace_oid_t> next_namespace_oid_;
  std::atomic<class_oid_t> next_class_oid_;
  std::atomic<attribute_oid_t> next_attribute_oid_;
  std::atomic<constraint_oid_t> next_constraint_oid_;

  std::atomic<timestamp_t> write_lock_;

  // cached data
  std::unique_ptr<access::Table> namespaces_;
  std::unique_ptr<access::BTreeIndex<TupleId>> namespaces_index_nspoid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> namespaces_index_nspname_;
  std::unique_ptr<access::DataChunkLayout> namespace_data_chunk_layout_;

  std::unique_ptr<access::Table> classes_;
  std::unique_ptr<access::BTreeIndex<TupleId>> classes_index_reloid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> classes_index_relname_;
  std::unique_ptr<access::BTreeIndex<TupleId>> classes_index_relnamespace_;
  std::unique_ptr<access::DataChunkLayout> classes_data_chunk_layout_;

  std::unique_ptr<access::Table> attributes_;
  std::unique_ptr<access::BTreeIndex<TupleId>> attributes_index_attnum_;
  std::unique_ptr<access::BTreeIndex<TupleId>> attributes_index_attrelid_attname_;
  std::unique_ptr<access::DataChunkLayout> attribute_data_chunk_layout_;

  std::unique_ptr<access::Table> indexes_;
  std::unique_ptr<access::BTreeIndex<TupleId>> indexes_index_indoid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> indexes_index_indrelid_;
  std::unique_ptr<access::DataChunkLayout> indexes_data_chunk_layout_;

  std::unique_ptr<access::Table> types_;
  std::unique_ptr<access::BTreeIndex<TupleId>> types_index_typoid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> types_index_typname_;
  std::unique_ptr<access::BTreeIndex<TupleId>> types_index_typnamespace_;

  std::unique_ptr<access::Table> constraints_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_conoid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_conname_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_connamespace_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_conrelid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_conindid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> constraints_index_confrelid_;
  std::unique_ptr<access::DataChunkLayout> constraint_data_chunk_layout_;

  std::unique_ptr<access::Table> languages_;
  std::unique_ptr<access::BTreeIndex<TupleId>> languages_index_lanoid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> languages_index_lanname_;

  std::unique_ptr<access::Table> procs_;
  std::unique_ptr<access::BTreeIndex<TupleId>> procs_index_prooid_;
  std::unique_ptr<access::BTreeIndex<TupleId>> procs_index_proname_;

  // Relation -> map of indexes, table (basically acts as a main class that manages heap)
  // graph where we store namespace.table.col_oid(1) <-> namespace.table.col_oid(2) basically maps
  // column deps. every time certa

  bool TryLock(transaction::TransactionContext *txn);

  ResultObj<namespace_oid_t> CreateNamespaceEntry(transaction::TransactionContext *txn,
                                                  const std::span<byte> name, namespace_oid_t oid);

  bool DeleteNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid);

  ResultObj<class_oid_t> CreateTableEntry(transaction::TransactionContext *txn,
                                          const std::span<byte> name, class_oid_t oid,
                                          namespace_oid_t namespace_oid, RelKind kind);

  ResultObj<attribute_oid_t> CreateColumnEntry(transaction::TransactionContext *txn,
                                               class_oid_t rel_oid, access::SchemaColumn &schema);

  bool DeleteTableEntry(transaction::TransactionContext *txn, class_oid_t oid);

  ResultObj<class_oid_t> CreateIndexEntry(transaction::TransactionContext *txn,
                                          const std::span<byte> name, class_oid_t class_oid,
                                          class_oid_t rel_oid, namespace_oid_t namespace_oid,
                                          access::IndexSchema &schema);

  ResultObj<class_oid_t> CreateConstraintEntry(transaction::TransactionContext *txn,
                                               constraint_oid_t oid, ConstraintProps props);

public:
  explicit DatabaseCatalog(catalog::db_oid_t db_id);
  ~DatabaseCatalog();

  idx_t NextOid() { return gen_oid_++; }

  const Identifier &GetName() const { return name_; }

  catalog::db_oid_t GetDbOid() const { return db_id_; }

  ResultObj<namespace_oid_t> CreateNamespace(transaction::TransactionContext *txn,
                                             const std::span<byte> name);

  bool DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid);

  ResultObj<void> ExistsNamespace(transaction::TransactionContext *txn, namespace_oid_t oid);

  bool UpdateNamespaceName(transaction::TransactionContext *txn, namespace_oid_t oid,
                           std::span<byte> name);

  ResultObj<class_oid_t> CreateTable(transaction::TransactionContext *txn,
                                     const std::span<byte> name, namespace_oid_t namespace_oid,
                                     access::Schema &schema);

  ResultObj<class_oid_t> CreateIndexClass(transaction::TransactionContext *txn,
                                          const std::span<byte> name, namespace_oid_t namespace_oid,
                                          access::Schema &schema);

  ResultObj<void> ExistsTable(transaction::TransactionContext *txn, class_oid_t oid);

  bool UpdateTableName(transaction::TransactionContext *txn, class_oid_t oid, std::span<byte> name,
                       namespace_oid_t namespace_oid);

  ResultObj<class_oid_t> CreateIndex(transaction::TransactionContext *txn,
                                     const std::span<byte> name, class_oid_t rel_oid,
                                     namespace_oid_t namespace_oid, access::IndexSchema &schema);

  ResultObj<class_oid_t> CreateConstraint(transaction::TransactionContext *txn,
                                          ConstraintProps props);

  void Select(transaction::TransactionContext *txn, int type);

  ResultObj<namespace_oid_t> GetNamespaceOid(transaction::TransactionContext *txn,
                                             const std::span<char> name) const;

  ResultObj<rel_oid_t> GetTableOid(transaction::TransactionContext *txn, const std::span<char> name,
                                   const namespace_oid_t ns_oid) const;

  // NEW:
private:
  optional_ptr<CatalogEntry> CreateSchemaInternal(transaction::TransactionContext &context,
                                                  Identifier &name);

public:
  DatabaseCatalog(Identifier name_);

  std::mutex &GetLock() { return mu; }

  // Finds schema by Identifier from CatalogSet
  optional_ptr<SchemaCatalogEntryBase> LookupSchema(transaction::TransactionContext &context,
                                                    const EntryLookupInfo &schema_lookup,
                                                    OnEntryNotFound if_not_found);

  // Finds schema by Identifier from CatalogSet
  optional_ptr<SchemaCatalogEntryBase> GetSchema(transaction::TransactionContext &context,
                                                 const Identifier &schema,
                                                 OnEntryNotFound if_not_found);

  optional_ptr<CatalogEntry> CreateSchema(transaction::TransactionContext &context,
                                          Identifier &name);

  // Get dependency manager
  optional_ptr<DependencyManager> GetDependencyManager();
};
} // namespace db7::catalog