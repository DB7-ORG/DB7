#include "catalog/database_catalog.hpp"
#include "transaction/transaction_util.hpp"

namespace db7::catalog
{

    bool DatabaseCatalog::TryLock(transaction::TransactionContext *txn)
    {
        auto current_val = write_lock_.load();
        if (current_val == txn->FinishTime())
        {
            return true;
        }

        if (transaction::TransactionUtil::HasConflict(current_val, txn->FinishTime(), txn->StartTime()))
        {
            txn->Abort();
            return false;
        }

        if (!write_lock_.compare_exchange_strong(current_val, txn->FinishTime()))
        {
            txn->Abort();
            return false;
        }

        // TODO register abort and commit actions here
        return true;
    }

    namespace_oid_t DatabaseCatalog::CreateNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid, const std::span<byte> name)
    {
        access::DataChunk *chunk = namespace_data_chunk_layout_->CreateDataChunk();

        access::DataChunkBuilder::BuildNamespaceChunk(chunk, oid, name);

        access::TupleId tup = namespaces_->Insert(txn, chunk);

        auto res_name = namespaces_index_nspname_->Insert(chunk, tup.value);
        if (!res_name.success)
        {
            return 0; // TODO fix index return proper result
        }

        auto res_oid = namespaces_index_nspoid_->Insert(chunk, tup.value);
        if (!res_oid.success)
        {
            return 0;
        }

        return oid;
    }

    namespace_oid_t DatabaseCatalog::CreateNamespace(transaction::TransactionContext *txn, const std::span<byte> name)
    {
        if (!TryLock(txn))
            return INVALID_OID;
        auto oid = next_namespace_oid_++;
        return CreateNamespaceEntry(txn, oid, name);
    }

    bool DatabaseCatalog::DeleteNamespaceEntry(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        (void)txn;
        (void)oid;
        return true;
    }

    bool DatabaseCatalog::DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        if (!TryLock(txn))
            return false;
        return DeleteNamespaceEntry(txn, oid);
    }

} // namespace db7::catalog
