
#include "common.hpp"

#include <atomic>
#include <emmintrin.h>

namespace db7::shared
{
    class AdaptiveSpinLock
    {
    private:
        std::atomic<u64> counter_;

    public:
        void Lock()
        {
            counter_++;
        }

        void Unlock()
        {
            counter_--;
        }

        void Try()
        {
            while (counter_.load() > 0)
            {
                _mm_pause();
            }
        }

        class SpinGuard
        {
            AdaptiveSpinLock &lock_;

        public:
            SpinGuard(AdaptiveSpinLock &lock) : lock_(lock) { lock_.Lock(); }
            ~SpinGuard() { lock_.Unlock(); }
            SpinGuard(const SpinGuard &) = delete;
            SpinGuard &operator=(const SpinGuard &) = delete;
        };
    };
}