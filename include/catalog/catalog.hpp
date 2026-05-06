#pragma once

#include "catalog/catalog_common.hpp"
#include "transaction/transaction_context.hpp"
#include "catalog/database_catalog.hpp"
#include "access/table.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows_builder.hpp"

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
     * Meaning its fully transactional and recoverable in case of errors
     */
    class Catalog
    {
    private:
        /**
         *  Next available database oid
         */
        access::Table databases_;
        std::unordered_map<db_oid_t, DatabaseCatalog *> databases_map_;
        std::atomic<db_oid_t> next_db_oid_;
        storage::BufferPool *buffer_pool_;
        storage::DiskManagerAsync *disk_mng_;

    public:
        Catalog(storage::BufferPool *buffer_pool, storage::DiskManagerAsync *disk_mng)
            : databases_(buffer_pool, disk_mng, Builder::CreateDatabaseSchema(), rel_oid_t(0), rel_oid_t(CatalogTableOid::PG_VARLEN)),
              databases_map_({}),
              next_db_oid_(catalog::db_oid_t(1)),
              buffer_pool_(buffer_pool),
              disk_mng_(disk_mng) {}

        db_oid_t CreateDatabase(db7::transaction::TransactionContext *txn, const std::span<byte> name, const bool bootstrap);

        bool CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc);
    };

}