#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"

namespace db7::catalog
{
    db_oid_t Catalog::CreateDatabase(db7::transaction::TransactionContext *txn, std::string &name, const bool bootstrap)
    {
        db_oid_t oid = next_db_oid_++;
        (void)oid;

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog();
        (void)dbc;

        // TODO register abort action in transaction ctx
        (void)txn;

        (void)bootstrap;

        (void)name;

        return 0;
    }
}