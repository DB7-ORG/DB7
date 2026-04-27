#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows.hpp"
#include "access/projected_rows_builder.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/varlen_entry.hpp"

#include <cstring>

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

        return db_oid_t(0);
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t db, const std::string &name, DatabaseCatalog *const dbc)
    {
        (void)dbc;
        (void)db;
        (void)txn;

        // TODO create varlen here with name
        // for now name len must be < 16 bytes
        // auto db_schema = databases_.GetSchema();

        // TODO this is really wierd
        pr_builder_.PrepareBuilder(1);
        pr_builder_.Push({next_db_oid_++});
        auto data = *(storage::VarlenEntry *)name.data();
        pr_builder_.Push({data});
        auto rows = pr_builder_.Build();

        databases_.Insert(rows);
        delete[] rows.data;

        return true;
    }
}