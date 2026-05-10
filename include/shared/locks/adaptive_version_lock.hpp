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

        // Reader: try optimistic first, fall back to shared lock
        u64 ReadOptimistic()
        {
            u64 s = seq.load(std::memory_order_acquire);
            if (!(s & 1))
                return s;
            return 0;
        }

        bool Validate(uint64_t s)
        {
            return seq.load(std::memory_order_acquire) == s;
        }
    };
}