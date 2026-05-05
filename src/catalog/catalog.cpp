#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows.hpp"
#include "access/projected_rows_builder.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/varlen_entry.hpp"
#include "shared/align_util.hpp"

#include <cstring>

namespace db7::catalog
{
    db_oid_t Catalog::CreateDatabase(transaction::TransactionContext *txn, std::string &name, const bool bootstrap)
    {
        db_oid_t oid = next_db_oid_++;

        DatabaseCatalog *dbc = Builder::CreateDatabaseCatalog(buffer_pool_, disk_mng_);
        databases_map_[oid] = dbc;

        CreateDatabaseEntry(txn, name, dbc);

        // TODO register abort action in transaction ctx
        (void)txn;

        (void)bootstrap;

        (void)name;

        return db_oid_t(0);
    }

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::string &name, DatabaseCatalog *const dbc) // TODO span
    {
        (void)dbc;
        (void)txn;

        // storage::VarlenEntry name()

        // TODO create varlen here with name
        // for now name len must be < 16 bytes
        auto db_schema = databases_.GetSchema();
        const u32 row_count = 1;
        u32 max_size = db_schema->CalculateMaxSize(row_count);
        auto block_owner = shared::AllocAligned(max_size, 16);
        auto *block = block_owner.get(); // TODO allocator

        access::ProjectedRowsBuilder pr_builder_(databases_.GetSchema(), block, row_count);
        pr_builder_.Push({next_db_oid_++});
        auto data = *(storage::VarlenEntry *)name.data();
        pr_builder_.Push({data});
        auto rows = pr_builder_.Build();

        databases_.Insert(rows);

        return true;
    }
}