#include "catalog/catalog.hpp"
#include "catalog/builder.hpp"
#include "access/projected_rows.hpp"
#include "access/projected_rows_builder.hpp"
#include "catalog/catalog_common.hpp"
#include "storage/varlen_entry.hpp"
#include "shared/align_util.hpp"
#include "access/access_builder.hpp"

#include <cstring>

namespace db7::catalog
{
    bool Catalog::RemoveMapping(db_oid_t oid)
    {
        auto it = databases_map_.find(oid);
        if (it == databases_map_.end())
        {
            return false;
        }
        delete it->second;
        databases_map_.erase(it);
        return true;
    }

    db_oid_t Catalog::CreateDatabase(transaction::TransactionContext *txn, const std::span<byte> name, const bool bootstrap)
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

    bool Catalog::CreateDatabaseEntry(transaction::TransactionContext *txn, const std::span<byte> name, DatabaseCatalog *const dbc)
    {
        db_oid_t oid = dbc->GetDbOid();
        (void)txn;

        storage::VarlenEntry entry = access::AccessBuilder::CreateVarlenEntry(name, databases_);

        auto db_schema = databases_->GetSchema();
        const u32 row_count = 1;

        u32 max_size = db_schema->CalculateMaxSize(row_count);
        auto block_owner = shared::AllocAligned(max_size, 16);
        auto *block = block_owner.get(); // TODO allocator

        access::ProjectedRowsBuilder pr_builder_(databases_->GetSchema(), block, row_count);
        pr_builder_.Push({oid});
        pr_builder_.Push({entry});
        auto rows = pr_builder_.Build();

        databases_->Insert(rows);

        // TODO insert real values
        // TODO CATALOG UNCOMMENT
        // databases_index_datoid->Insert(oid, 0);
        // databases_index_datname->Insert(oid, 0);

        return true;
    }

    bool Catalog::DeleteDatabase(transaction::TransactionContext *txn, const db_oid_t oid)
    {
        (void)txn;

        if (!DeleteDatabaseEntry(txn, oid))
        {
            DB7_ASSERT(false, "Failed to delete entry");
            return false;
        }

        if (!RemoveMapping(oid))
        {
            DB7_ASSERT(false, "Mapping not found");
            return false;
        }

        return true;
    }

    bool Catalog::DeleteDatabaseEntry(transaction::TransactionContext *txn, const db_oid_t oid)
    {
        (void)oid;
        (void)txn;

        // scan the index of the table and get the idx of the row
        u32 idx = 0;
        u32 pid = 1;

        databases_->Delete(idx, pid);

        return true;
    }
}