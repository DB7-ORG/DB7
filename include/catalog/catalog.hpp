#pragma once

#include "catalog/catalog_common.hpp"
#include "transaction/transaction_context.hpp"
#include "catalog/database_catalog.hpp"
#include "access/table.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows_builder.hpp"
#include "access/index/btree_index.hpp"

#include <atomic>
#include <unordered_map>

namespace storage
{
    class BufferPool; // forward declare, no #include needed
}

namespace db7::catalog
{
    class DatabaseCatalog;

    /**
     * This component manages all metadata for databases. There is lower component DatabaseCatalog that manages tables, schemas...
     * Its implemented using relational model and all operations inside it should work just like any other table operation.
     * Meaning its fully transactional and recoverable in case of errors.
     * There is only a single catalog instance at a time.
     */
    class Catalog
    {
    private:
        access::Table *databases_;
        access::BTreeIndex<u64> *databases_index_datoid;
        access::BTreeIndex<access::Key> *databases_index_datname;
        std::unordered_map<db_oid_t, DatabaseCatalog *> databases_map_;
        std::atomic<db_oid_t> next_db_oid_;
        storage::BufferPool *buffer_pool_;
        storage::DiskManagerAsync *disk_mng_;

        /**
         * Removes from databases_map_
         * @param oid - key that needs to be returned
         * @result
         *  true if entry is removed
         *  false entry not found
         */
        bool RemoveMapping(db_oid_t oid);

        /**
         * Inserts a tuple to system database table.
         * @param txn transaction context that manages transaction operations.
         * @param name database name
         * @param dbc database catalog instance
         * @result success flag
         */
        bool CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc);

        /**
         * Deletes a tuple from a system database table.
         * @param txn transaction context that manages transaction operations.
         * @param oid database id
         * @result success flag
         */
        bool DeleteDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t oid);

    public:
        /**
         * Creates catalog table.
         * This should newer be called in regular code other than at startup..
         */
        Catalog(storage::BufferPool *buffer_pool, storage::DiskManagerAsync *disk_mng)
            : databases_map_({}),
              next_db_oid_(catalog::db_oid_t(1)),
              buffer_pool_(buffer_pool),
              disk_mng_(disk_mng)
        {
            databases_ = new access::Table(buffer_pool, disk_mng, Builder::CreateDatabaseSchema(), rel_oid_t(CatalogTableOid::PG_DATABASES), rel_oid_t(CatalogTableOid::PG_VARLEN));
            databases_index_datoid = new access::BTreeIndex<u64>(buffer_pool, disk_mng, rel_oid_t(CatalogTableOid::PG_INDEX_DATABASE_DATOID));
            databases_index_datname = new access::BTreeIndex<access::Key>(buffer_pool, disk_mng, rel_oid_t(CatalogTableOid::PG_INDEX_DATABASE_DATNAME));
        }

        ~Catalog()
        {
            for (auto &[oid, dbc] : databases_map_)
                delete dbc;
            delete databases_;
            delete databases_index_datoid;
            delete databases_index_datname;
        }

        /**
         * Creates bunch of systems table and a database catalog instance that manages them.
         * Inserts and entry to the pg_database table that tracks database instances.
         * @param txn transaction context that manages transaction operations.
         * @param name database name
         * @param bootstrap indicates whether or not to perform bootstrap routine
         * @result id of a new database instance
         */
        db_oid_t CreateDatabase(db7::transaction::TransactionContext *txn, const std::span<byte> name, const bool bootstrap);

        /**
         * Reverses everything CreateDatabase did
         * @param txn transaction context that manages transaction operations.
         * @param oid database id
         * @result success flag
         */
        bool DeleteDatabase(transaction::TransactionContext *txn, db_oid_t oid);
    };

}