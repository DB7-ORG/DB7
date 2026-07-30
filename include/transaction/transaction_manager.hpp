#pragma once

#include "transaction/timestamp_manager.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "transaction/transaction_context.hpp"
#include "shared/arena/object_pool.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "shared/locks/adaptive_spin_lock.hpp"

namespace db7::transaction
{
    class TransactionManager
    {
    private:
        TimestampManager *timestamp_manager_;
        storage::BufferPool *buffer_pool_;
        storage::PageVersionManager *version_manager_;
        shared::ObjectPool<shared::FixedBumpArena> *mem_pool_;
        shared::AdaptiveSpinLock commit_latch_;

    public:
        TransactionManager(
            TimestampManager *timestamp_manager,
            storage::BufferPool *buffer_pool,
            storage::PageVersionManager *version_manager,
            shared::ObjectPool<shared::FixedBumpArena> *mem_pool)
            : timestamp_manager_(timestamp_manager), buffer_pool_(buffer_pool), version_manager_(version_manager), mem_pool_(mem_pool) {}

        /**
         * Begins a transaction.
         * @return transaction context for the newly begun transaction
         */
        TransactionContext *BeginTransaction();

        timestamp_t Commit(TransactionContext *txn);
    };
}