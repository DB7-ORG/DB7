#pragma once

#include "catalog/catalog_common.hpp"
#include "transaction/transaction_context.hpp"

#include <atomic>

namespace db7::catalog
{
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
        std::atomic<db_oid_t> next_db_oid_;

    public:
        db_oid_t CreateDatabase(db7::transaction::TransactionContext *txn, std::string &name, const bool bootstrap);
    };

}