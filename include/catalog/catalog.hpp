#pragma once

#include "catalog/catalog_common.hpp"
#include "transaction/transaction_context.hpp"
#include "catalog/database_catalog.hpp"
#include "access/table.hpp"
#include "catalog/builder.hpp"

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

    public:
        Catalog(storage::BufferPool *buffer_pool)
            : databases_(buffer_pool, Builder::CreateDatabaseSchema(), rel_oid_t{0}),
              databases_map_({}),
              next_db_oid_(1),
              buffer_pool_(buffer_pool) {}

        db_oid_t CreateDatabase(db7::transaction::TransactionContext *txn, std::string &name, const bool bootstrap);

        bool CreateDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t db, const std::string &name, DatabaseCatalog *const dbc);
    };

}