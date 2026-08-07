#pragma once
//
// tests/support/concurrency.hpp
//
// Generic helpers for concurrent tests. Nothing here knows about the B-tree —
// it should be usable from buffer-pool, catalog, or transaction tests too.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace db7::test
{
    inline unsigned DefaultThreads()
    {
        const unsigned n = std::thread::hardware_concurrency();
        return n ? n : 4;
    }

    // =========================================================================
    // Work partitioning
    // =========================================================================

    struct Range
    {
        size_t lo = 0;
        size_t hi = 0;

        size_t size() const { return hi > lo ? hi - lo : 0; }
        bool empty() const { return size() == 0; }
    };

    /// Splits [offset, total) into `nthreads` contiguous chunks and returns
    /// the one belonging to thread `t`. Chunks never overlap, so a test that
    /// gives each thread its own Range gets a deterministic expected end state.
    inline Range PartitionRange(unsigned t, unsigned nthreads, size_t total,
                                size_t offset = 0)
    {
        const size_t measured = total > offset ? total - offset : 0;
        const size_t per = (measured + nthreads - 1) / nthreads;
        const size_t lo = std::min(offset + t * per, total);
        const size_t hi = std::min(lo + per, total);
        return {lo, hi};
    }

    // =========================================================================
    // Parallel runner
    // =========================================================================

    /// Spawns `nthreads` threads running fn(t), all released from a spin
    /// barrier simultaneously so the measured window excludes thread startup
    /// and every thread is genuinely contending from the first instruction.
    /// Returns elapsed nanoseconds.
    template <typename Fn>
    double RunParallel(unsigned nthreads, Fn &&fn)
    {
        std::atomic<unsigned> ready{0};
        std::atomic<bool> go{false};

        std::vector<std::thread> workers;
        workers.reserve(nthreads);

        for (unsigned t = 0; t < nthreads; ++t)
        {
            workers.emplace_back(
                [&, t]
                {
                    ready.fetch_add(1, std::memory_order_acq_rel);
                    while (!go.load(std::memory_order_acquire))
                    {
                        /* spin */
                    }
                    fn(t);
                });
        }

        while (ready.load(std::memory_order_acquire) < nthreads)
        {
            /* spin */
        }

        const auto t0 = std::chrono::steady_clock::now();
        go.store(true, std::memory_order_release);
        for (auto &w : workers)
        {
            w.join();
        }
        const auto t1 = std::chrono::steady_clock::now();

        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }

    /// Runs two different workloads concurrently — e.g. readers against
    /// writers. Thread indices passed to each fn are 0-based within that group.
    template <typename FnA, typename FnB>
    double RunParallelMixed(unsigned n_a, FnA &&fn_a, unsigned n_b, FnB &&fn_b)
    {
        return RunParallel(n_a + n_b,
                           [&](unsigned t)
                           {
                               if (t < n_a)
                                   fn_a(t);
                               else
                                   fn_b(t - n_a);
                           });
    }

    // =========================================================================
    // Thread-safe failure collection
    //
    // gtest's ASSERT_* macros expand to `return;`, so they silently do the
    // wrong thing inside a worker lambda, and reporting a failure from a
    // non-main thread is not portable. Workers record failures here; the main
    // thread calls ExpectNoFailures() after joining.
    // =========================================================================

    class ErrorLog
    {
    public:
        static constexpr size_t kMaxMessages = 20;

        void Fail(std::string message)
        {
            count_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> guard(mutex_);
            if (messages_.size() < kMaxMessages)
            {
                messages_.push_back(std::move(message));
            }
        }

        void FailIf(bool condition, std::string message)
        {
            if (condition)
            {
                Fail(std::move(message));
            }
        }

        /// Stream-style: log.Failf("key ", i, " missing, got ", n, " rids");
        template <typename... Args>
        void Failf(Args &&...parts)
        {
            std::ostringstream os;
            (os << ... << std::forward<Args>(parts));
            Fail(os.str());
        }

        size_t Count() const { return count_.load(std::memory_order_relaxed); }
        bool Empty() const { return Count() == 0; }

        std::vector<std::string> Messages() const
        {
            std::lock_guard<std::mutex> guard(mutex_);
            return messages_;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<std::string> messages_;
        std::atomic<size_t> count_{0};
    };

    /// Call from the main thread after joining. Reports up to kMaxMessages
    /// distinct failures plus the total count.
    inline void ExpectNoFailures(const ErrorLog &log)
    {
        if (log.Empty())
        {
            return;
        }

        std::ostringstream os;
        os << log.Count() << " failure(s) across worker threads:";
        for (const auto &m : log.Messages())
        {
            os << "\n  - " << m;
        }
        if (log.Count() > ErrorLog::kMaxMessages)
        {
            os << "\n  ... " << (log.Count() - ErrorLog::kMaxMessages) << " more";
        }
        ADD_FAILURE() << os.str();
    }

    // =========================================================================
    // Repetition
    // =========================================================================

    /// Races are probabilistic: a single pass proves very little. Repeat runs
    /// the body `times` times and tags each iteration in the failure output.
    template <typename Fn>
    void Repeat(int times, Fn &&fn)
    {
        for (int i = 0; i < times; ++i)
        {
            SCOPED_TRACE(::testing::Message() << "iteration " << i);
            fn(i);
            if (::testing::Test::HasFatalFailure())
            {
                return;
            }
        }
    }

} // namespace db7::test