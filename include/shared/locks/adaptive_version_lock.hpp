#include "common.hpp"

#include <atomic>
#include <shared_mutex>

namespace db7::shared
{
    class AdaptiveVersionLock
    {
    private:
        std::atomic<u64> seq{0};
        std::shared_mutex rw_mtx;

    public:
        void WriteLock()
        {
            rw_mtx.lock();
            seq.fetch_add(1, std::memory_order_release);
        }

        void WriteUnlock()
        {
            seq.fetch_add(1, std::memory_order_release);
            rw_mtx.unlock();
        }

        void ReadLock()
        {
            rw_mtx.lock_shared();
        }

        void ReadUnlock()
        {
            rw_mtx.unlock_shared();
        }

        bool ReadOptimistic(u64 &version)
        {
            version = seq.load(std::memory_order_acquire);
            if (!(version & 1))
                return true;
            return false;
        }

        bool Validate(u64 s)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            return seq.load(std::memory_order_relaxed) == s;
        }
    };
}