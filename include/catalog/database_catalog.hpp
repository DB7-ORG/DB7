#pragma once

#include "access/table.hpp"
#include "access/index/btree.hpp"
#include "shared/models/tuple_id.hpp"
#include "shared/models/result_object.hpp"
#include "catalog/builder.hpp"
#include "access/index_schema.hpp"

#include <vector>
#include <atomic>

namespace db7::catalog
{
    class Builder;

    /**
     * Database catalog is a component managed by db7::catalog::Catalog.
     * Catalog isnt managing this component because the database lifetime is strongly tied to DatabaseCatalog lifetime.
     * Meaning this object exists as long as the database.
     * This component stores cached catalog entries. They can be evicetd only using dll modifications when cache is invalidated.
     */
    class DatabaseCatalog
    {
    private:
        friend class Builder;

        catalog::db_oid_t db_id_;

        std::atomic<namespace_oid_t> next_namespace_oid_;
        std::atomic<class_oid_t> next_class_oid_;
        std::atomic<attribute_oid_t> next_attribute_oid_;
        std::atomic<constraint_oid_t> next_constraint_oid_;

        std::atomic<transaction::timestamp_t> write_lock_;

        bool TryLock(transaction::TransactionContext *txn);

        ResultObj<namespace_oid_t> CreateNamespaceEntry(transaction::TransactionContext *txn, const std::span<byte> name, namespace_oid_t oid);

        bool DeleteNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid);

        ResultObj<class_oid_t> CreateTableEntry(transaction::TransactionContext *txn, const std::span<byte> name, class_oid_t oid, namespace_oid_t namespace_oid);

        ResultObj<attribute_oid_t> CreateColumnEntry(transaction::TransactionContext *txn, class_oid_t rel_oid, access::SchemaColumn &schema);

        bool DeleteTableEntry(transaction::TransactionContext *txn, class_oid_t oid);

        ResultObj<class_oid_t> CreateIndexEntry(
            transaction::TransactionContext *txn,
            const std::span<byte> name,
            class_oid_t class_oid,
            class_oid_t rel_oid,
            namespace_oid_t namespace_oid,
            access::IndexSchema &schema);

        ResultObj<class_oid_t> CreateConstraintEntry(
            transaction::TransactionContext *txn,
            constraint_oid_t oid,
            ConstraintProps props);

        // cached data
        access::Table *namespaces_;
        access::BTreeIndex<TupleId> *namespaces_index_nspoid_;
        access::BTreeIndex<TupleId> *namespaces_index_nspname_;
        access::DataChunkLayout *namespace_data_chunk_layout_;

        access::Table *classes_;
        access::BTreeIndex<TupleId> *classes_index_reloid_;
        access::BTreeIndex<TupleId> *classes_index_relname_;
        access::BTreeIndex<TupleId> *classes_index_relnamespace_;
        access::DataChunkLayout *classes_data_chunk_layout_;

        access::Table *attributes_;
        access::BTreeIndex<TupleId> *attributes_index_attnum_;
        access::BTreeIndex<TupleId> *attributes_index_attrelid_attname_;
        access::DataChunkLayout *attribute_data_chunk_layout_;

        access::Table *indexes_;
        access::BTreeIndex<TupleId> *indexes_index_indoid_;
        access::BTreeIndex<TupleId> *indexes_index_indrelid_;
        access::DataChunkLayout *indexes_data_chunk_layout_;

        access::Table *types_;
        access::BTreeIndex<TupleId> *types_index_typoid_;
        access::BTreeIndex<TupleId> *types_index_typname_;
        access::BTreeIndex<TupleId> *types_index_typnamespace_;

        access::Table *constraints_;
        access::BTreeIndex<TupleId> *constraints_index_conoid_;
        access::BTreeIndex<TupleId> *constraints_index_conname_;
        access::BTreeIndex<TupleId> *constraints_index_connamespace_;
        access::BTreeIndex<TupleId> *constraints_index_conrelid_;
        access::BTreeIndex<TupleId> *constraints_index_conindid_;
        access::BTreeIndex<TupleId> *constraints_index_confrelid_;
        access::DataChunkLayout *constraint_data_chunk_layout_;

        access::Table *languages_;
        access::BTreeIndex<TupleId> *languages_index_lanoid_;
        access::BTreeIndex<TupleId> *languages_index_lanname_;

        access::Table *procs_;
        access::BTreeIndex<TupleId> *procs_index_prooid_;
        access::BTreeIndex<TupleId> *procs_index_proname_;

    public:
        DatabaseCatalog(catalog::db_oid_t db_id)
            : db_id_(db_id),
              next_namespace_oid_(1),
              next_class_oid_(1),
              next_attribute_oid_(1),
              next_constraint_oid_(1)
        {
        }

        ~DatabaseCatalog()
        {
            delete namespaces_;
            delete namespaces_index_nspoid_;
            delete namespaces_index_nspname_;

            delete classes_;
            delete classes_index_reloid_;
            delete classes_index_relname_;
            delete classes_index_relnamespace_;

            delete attributes_;
            delete attributes_index_attnum_;
            delete attributes_index_attrelid_attname_;

            delete types_;
            delete types_index_typoid_;
            delete types_index_typname_;
            delete types_index_typnamespace_;

            delete constraints_;
            delete constraints_index_conoid_;
            delete constraints_index_conname_;
            delete constraints_index_connamespace_;
            delete constraints_index_conrelid_;
            delete constraints_index_conindid_;
            delete constraints_index_confrelid_;

            delete languages_;
            delete languages_index_lanoid_;
            delete languages_index_lanname_;

            delete procs_;
            delete procs_index_prooid_;
            delete procs_index_proname_;
        }

        catalog::db_oid_t GetDbOid() const { return db_id_; }

        ResultObj<namespace_oid_t> CreateNamespace(transaction::TransactionContext *txn, const std::span<byte> name);

        bool DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid);

        ResultObj<void> ExistsNamespace(transaction::TransactionContext *txn, namespace_oid_t oid);

        bool UpdateNamespaceName(transaction::TransactionContext *txn, namespace_oid_t oid, std::span<byte> name);

        ResultObj<class_oid_t> CreateTable(transaction::TransactionContext *txn, const std::span<byte> name, namespace_oid_t namespace_oid, access::Schema &schema);

        ResultObj<void> ExistsTable(transaction::TransactionContext *txn, class_oid_t oid);

        bool UpdateTableName(transaction::TransactionContext *txn, class_oid_t oid, std::span<byte> name, namespace_oid_t namespace_oid);

        ResultObj<class_oid_t> CreateIndex(
            transaction::TransactionContext *txn,
            const std::span<byte> name,
            class_oid_t rel_oid,
            namespace_oid_t namespace_oid,
            access::IndexSchema &schema);

        ResultObj<class_oid_t> CreateConstraint(transaction::TransactionContext *txn, ConstraintProps props);

        void Select(transaction::TransactionContext *txn, int type);
    };
}