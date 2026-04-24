#pragma once

#include "common.hpp"

#include <atomic>
#include <immintrin.h>
#include <thread>

// TODO Revisit this can be interesting
namespace db7::shared
{
    struct alignas(64) AdaptiveRWLock
    {
        std::atomic<i32> state{0};
        static constexpr int SPIN_COUNT = 100;

        void lock_shared()
        {
            for (;;)
            {
                // Spin phase
                for (int i = 0; i < SPIN_COUNT; i++)
                {
                    i32 s = state.load(std::memory_order_relaxed);
                    if (s >= 0 && state.compare_exchange_weak(s, s + 1, std::memory_order_acquire))
                        return;
                    _mm_pause();
                }
                // Back off to OS if spinning failed
                std::this_thread::yield();
            }
        }

        void unlock_shared()
        {
            state.fetch_sub(1, std::memory_order_release);
        }

        void lock()
        {
            for (;;)
            {
                for (int i = 0; i < SPIN_COUNT; i++)
                {
                    i32 expected = 0;
                    if (state.compare_exchange_weak(expected, -1, std::memory_order_acquire))
                        return;
                    _mm_pause();
                }
                std::this_thread::yield();
            }
        }

        void unlock()
        {
            state.store(0, std::memory_order_release);
        }
    };
}