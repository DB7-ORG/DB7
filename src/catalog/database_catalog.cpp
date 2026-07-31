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
        storage::VarlenEntry entry;
        entry.Set(name);

        access::DataChunk *chunk = namespace_data_chunk_layout_->CreateDataChunk();
        chunk->Write(catalog::col_oid_t(CatalogColumnOid::NSPOID), oid);
        chunk->Write(catalog::col_oid_t(CatalogColumnOid::NSPNAME), entry);
        access::TupleId tup = namespaces_->Insert(txn, chunk);

        // TODO fix this
        byte *buf = new byte[name.size() * 16];
        u16 len = access::KeyNormEncoder::Encode(buf, name, false, false, false);
        auto k = access::Key{(u16)name.size(), name.data(), len, buf};
        namespaces_index_nspname_->Insert(k, tup.value);

        namespaces_index_nspoid_->Insert(oid, tup.value);

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
    }

    bool DatabaseCatalog::DeleteNamespace(transaction::TransactionContext *txn, namespace_oid_t oid)
    {
        if (!TryLock(txn))
            return false;
        return DeleteNamespaceEntry(txn, oid);
    }

} // namespace db7::catalog
