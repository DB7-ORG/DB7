#pragma once

#include "access/table.hpp"
#include "access/index/index.hpp"
#include "access/index/btree_index.hpp"

#include <vector>
#include <atomic>

namespace db7::catalog
{
    /**
     * Database catalog is a component managed by db7::catalog::Catalog.
     * Catalog isnt managing this component because the database lifetime is strongly tied to DatabaseCatalog lifetime.
     * Meaning this object exists as long as the database.
     * This component stores cached catalog entries. They can be evicetd only using dll modifications when cache is invalidated.
     */
    class DatabaseCatalog
    {
    private:
        catalog::db_oid_t db_id_;

        std::atomic<namespace_oid_t> next_namespace_oid_;

        std::atomic<transaction::timestamp_t> write_lock_;

        bool TryLock(transaction::TransactionContext *txn);

        namespace_oid_t CreateNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid, const std::span<byte> name);

        bool DeleteNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid);

    public:
        // cached data
        access::Table *namespaces_;
        access::BTreeIndex<u32> *namespaces_index_nspoid_;
        access::BTreeIndex<access::Key> *namespaces_index_nspname_;
        access::DataChunkLayout *namespace_data_chunk_layout_;

        access::Table *classes_;
        access::Index *classes_index_reloid_;
        access::Index *classes_index_relname_;
        access::Index *classes_index_relnamespace_;

        access::Table *attributes_;
        access::Index *attributes_index_attnum_;
        access::Index *attributes_index_attrelid_;
        access::Index *attributes_index_attname_;

        access::Table *types_;
        access::Index *types_index_typoid_;
        access::Index *types_index_typname_;
        access::Index *types_index_typnamespace_;

        access::Table *constraints_;
        access::Index *constraints_index_conoid_;
        access::Index *constraints_index_conname_;
        access::Index *constraints_index_connamespace_;
        access::Index *constraints_index_conrelid_;
        access::Index *constraints_index_conindid_;
        access::Index *constraints_index_confrelid_;

        access::Table *languages_;
        access::Index *languages_index_lanoid_;
        access::Index *languages_index_lanname_;

        access::Table *procs_;
        access::Index *procs_index_prooid_;
        access::Index *procs_index_proname_;

        DatabaseCatalog(catalog::db_oid_t db_id) : db_id_(db_id) {}

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
            delete attributes_index_attrelid_;
            delete attributes_index_attname_;

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

        catalog::db_oid_t GetDbOid()
        {
            return db_id_;
        }

        namespace_oid_t CreateNamespace(transaction::TransactionContext *txn, const std::span<byte> name);

        bool DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid);
    };
}