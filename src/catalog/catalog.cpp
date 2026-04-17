#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"

namespace db7::catalog
{
    db_oid_t Catalog::CreateDatabase(transaction::TransactionContext *txn, std::string &name, const bool bootstrap)
    {
        db_oid_t oid = next_db_oid_++;

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_);
        databases_map_[oid] = dbc;

        // TODO register abort action in transaction ctx
        (void)txn;

        (void)bootstrap;

        (void)name;

        return 0;
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t db, const std::string &name, DatabaseCatalog *const dbc)
    {
        // crate varlen entry

        databases_.Insert();
    }
}