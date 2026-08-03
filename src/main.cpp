#include <iostream>

#include "common.hpp"
#include <fmt/core.h>
#include <random>
#include <barrier>
// #include <thread>
// #include <chrono>

#include "catalog/catalog.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "debug/printer.hpp"
#include "access/index/btree.hpp"
#include "shared/error/exception.hpp"
#include "shared/arena/object_pool.hpp"
#include "shared/arena/fixed_bump_arena.hpp"
#include "transaction/transaction_manager.hpp"
#include "shared/arena/object_pool.hpp"
#include "shared/arena/fixed_bump_arena.hpp"

using namespace db7;

static inline u64 now_ns()
{
    std::atomic_signal_fence(std::memory_order_seq_cst);

    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    std::atomic_signal_fence(std::memory_order_seq_cst);

    return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

constexpr u32 PAGES = 10000;

void populate_table()
{
    int fd = open(".data/table_2.db", O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (fd < 0)
    {
        DB7_ASSERT(false, "File not found");
        return;
    }

    alignas(4096) u8 buf[DB7_PAGE_SIZE] = {};
    for (u32 i = 0; i < PAGES; i++)
    {
        memset(buf, 0, DB7_PAGE_SIZE);
        *reinterpret_cast<u64 *>(buf) = i;
        ssize_t written = pwrite(fd, buf, DB7_PAGE_SIZE, (off_t)i * DB7_PAGE_SIZE);
        if (written != DB7_PAGE_SIZE)
        {
            fmt::print("pwrite failed: %s", strerror(errno));
            // return error or abort depending on your strategy
        }
    }
    fsync(fd);
}

void test_buffer_pool(db7::storage::BufferPool &buffer_pool)
{
    constexpr u32 NUM_THREADS = 500;
    constexpr u32 NUM_OPS = 500;

    u64 t00 = now_ns();

    std::vector<std::thread> threads;
    for (u32 t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&buffer_pool, t]()
                             {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<u32> dist(0, PAGES-1);

        for (u32 i = 0; i < NUM_OPS; i++)
        {
            db7::storage::PageIdentifier id(2, dist(rng));
            auto page = buffer_pool.Pin(id);

            page->WaitIO();

            //std::this_thread::sleep_for(std::chrono::milliseconds(1));

            buffer_pool.Unpin(page);
        } });
    }

    for (auto &t : threads)
        t.join();

    u64 t0 = now_ns();

    shared::Print(buffer_pool);

    printf("time:        %.3f ms\n", (t0 - t00) / 1e6);
}

// void test_index_perf(db7::storage::BufferPool *buffer_pool, db7::storage::DiskManagerAsync *disk_mng_async)
// {
//     using Typ = access::Key;
//     db7::access::BTreeIndex<Typ> index(buffer_pool, disk_mng_async, 100);

//     u32 n = 100'000;
//     // std::vector<u32> keys(n);
//     // std::iota(keys.begin(), keys.end(), 1);
//     // std::shuffle(keys.begin(), keys.end(), std::mt19937{42});

//     std::vector<db7::access::Key> strs(n);
//     for (u32 i = 0; i < n; i++)
//     {
//         std::string s = "kEY_ⅶ_⎞_Љ_۝_" + std::to_string(i + 1);
//         byte *buf = new byte[s.size() * 16];
//         byte *raw = new byte[s.size()]; // ← own copy of raw too

//         std::memcpy(raw, s.data(), s.size());

//         std::span sp((const byte *)s.data(), (u16)s.size());
//         u32 len = access::KeyNormEncoder::Encode(buf, sp, false, false, false);

//         strs[i].data = raw;
//         strs[i].len = (u16)s.size();
//         strs[i].encoded = buf;
//         strs[i].enc_len = (u16)(len);
//     }
//     std::shuffle(strs.begin(), strs.end(), std::mt19937{42});

//     std::vector<db7::access::Key> keys(n);
//     for (u32 i = 0; i < n; i++)
//     {
//         keys[i] = strs[i];
//     }

//     u32 num_threads = std::thread::hardware_concurrency();
//     std::cout << num_threads << std::endl;

//     bool SINGLE_THREAD = true;

//     std::vector<std::thread> threads(num_threads);
//     std::barrier sync_point(num_threads + 1);
//     std::vector<std::thread> read_threads(num_threads);
//     std::barrier read_sync(num_threads + 1);

//     if (!SINGLE_THREAD)
//     {
//         for (u32 t = 0; t < num_threads; t++)
//         {
//             threads[t] = std::thread([&, t]()
//                                      {
//         u32 start = (n * t) / num_threads;
//         u32 end = (n * (t + 1)) / num_threads;

//         sync_point.arrive_and_wait(); // wait for all threads ready

//         for (u32 i = start; i < end; i++)
//             index.Insert(keys[i], i); });
//         }

//         for (u32 t = 0; t < num_threads; t++)
//         {
//             read_threads[t] = std::thread([&, t]()
//                                           {
//         u32 start = (n * t) / num_threads;
//         u32 end = (n * (t + 1)) / num_threads;

//         read_sync.arrive_and_wait();

//         for (u32 i = start; i < end; i++)
//         {
//             auto item = index.Get(keys[i]);
//             if (item != i)
//                 throw std::runtime_error("value doesnt match");
//         } });
//         }
//     }

//     u64 t0 = now_ns();

//     if (!SINGLE_THREAD)
//     {
//         sync_point.arrive_and_wait(); // release all threads

//         for (auto &th : threads)
//             th.join();
//     }
//     else
//     {
//         for (u32 i = 0; i < n; i++)
//         {
//             // std::cout << i << std::endl;
//             index.Insert(keys[i], i);
//         }
//     }

//     u64 t1 = now_ns();

//     if (!SINGLE_THREAD)
//     {
//         read_sync.arrive_and_wait();

//         for (auto &th : read_threads)
//             th.join();
//     }
//     else
//     {
//         for (u32 i = 0; i < n; i++)
//         {
//             auto item = index.Get(keys[i]);
//             if (item != i)
//             {
//                 throw std::runtime_error("value doesnt match");
//             }
//         }
//     }

//     u64 t2 = now_ns();

//     for (u32 i = 0; i < n; i++)
//     {
//         delete[] strs[i].data;
//         delete[] strs[i].encoded;
//     }

//     // db7::shared::Print(*buffer_pool);

//     printf("insert:        %.3f ms\n", (t1 - t0) / 1e6);
//     printf("search:        %.3f ms\n", (t2 - t1) / 1e6);
// }

db7::storage::BufferPool *g_buffer_pool;
db7::catalog::Catalog *g_catalog;

// int main()
// {
//     fmt::print("Hello, {}!\n", "world");

//     // // populate_table();

//     db7::storage::DiskManagerAsync disk_mng_async(".data");
//     disk_mng_async.CreateOpenFile(1, 3);
//     // disk_mng_async.TruncateFile(2, PAGES);

//     db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
//     disk_scheduler.Start();

//     db7::storage::PageVersionManager version_manager;

//     db7::storage::BufferPool buffer_pool(&disk_scheduler, &version_manager);
//     g_buffer_pool = &buffer_pool;

//     db7::transaction::TimestampManager timestamp_manager;

//     db7::shared::ObjectPool<shared::FixedBumpArena> pool(10'000, 2000);

//     db7::transaction::TransactionManager txn_manager(&timestamp_manager, &buffer_pool, &version_manager, &pool);

//     db7::transaction::TransactionContext *context = txn_manager.BeginTransaction();

//     // test_index_perf(&buffer_pool, &disk_mng_async);

//     auto cat = new catalog::Catalog(&buffer_pool, &disk_mng_async);
//     g_catalog = cat;

//     std::string s = "ssss";
//     std::span<byte> sdata(reinterpret_cast<byte *>(s.data()), s.size());

//     auto db_oid = cat->CreateDatabase(context, sdata, true);
//     // cat->DeleteDatabase(context, db_oid);
//     cat->Select(context);

//     std::string new_name = "jovo";
//     std::cout << cat->UpdateDatabaseName(context, db_oid, std::span<char>(new_name.data(), new_name.size())) << std::endl;

//     cat->Select(context);

//     txn_manager.Commit(context);

//     // cat->CreateDatabase(nullptr, sdataa, true);

//     // std::this_thread::sleep_for(std::chrono::seconds(1));

//     db7::transaction::TransactionContext *context2 = txn_manager.BeginTransaction();

//     std::string new_name2 = "jovo222";
//     std::cout << cat->UpdateDatabaseName(context2, db_oid, std::span<char>(new_name2.data(), new_name2.size())) << std::endl;

//     cat->Select(context2);

//     db7::transaction::TransactionContext *context3 = txn_manager.BeginTransaction();
//     cat->Select(context3);
//     txn_manager.Commit(context3);

//     txn_manager.Commit(context2);

//     delete cat;

//     disk_scheduler.Stop();

//     return 0;
// }

#include "common.hpp"
#include "access/index/btree.hpp"

#include <fmt/core.h>
#include <cstring>
#include <vector>
#include <span>
#include <random>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <thread>

using namespace db7;

// static inline u64 now_ns()
// {
//     timespec ts;
//     clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
//     return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
// }

template <typename T>
void prep_keys(std::vector<access::Key> &strs, u32 n)
{
    // if constexpr (std::is_same_v<T, Key>)
    // {
    //     for (u32 i = 0; i < n; i++)
    //     {
    //         std::string s = "kEY_ⅶ_⎞_Љ_۝_" + std::to_string(i + 1);
    //         byte *buf = new byte[s.size() * 16];
    //         byte *raw = new byte[s.size()]; // ← own copy of raw too

    //         std::memcpy(raw, s.data(), s.size());

    //         std::span sp((const byte *)s.data(), (u16)s.size());
    //         u32 len = KeyNormEncoder::Encode(buf, sp, false, false, false);

    //         strs[i].data = raw;
    //         strs[i].len = (u16)s.size();
    //         strs[i].encoded = buf;
    //         strs[i].enc_len = (u16)(len);
    //     }
    // }
    // else
    // {
    for (u64 i = 0; i < n; i++)
    {
        byte *buf = new byte[sizeof(u64) * 16];

        u32 len = access::KeyNormEncoder::Encode(buf, i, false, false, false);

        std::memcpy(buf + len, &i, sizeof(u64));

        strs[i].data = buf;
        strs[i].len = len + sizeof(u64);
        strs[i].enc_len = (u16)(len);
    }
    //}
}

template <typename Fn>
inline double run_parallel(unsigned nthreads, Fn &&fn)
{
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> ts;
    ts.reserve(nthreads);

    for (unsigned t = 0; t < nthreads; t++)
    {
        ts.emplace_back([&, t]
                        {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (!go.load(std::memory_order_acquire)) { /* spin */
            }
            fn(t); });
    }
    while (ready.load(std::memory_order_acquire) < nthreads)
    { /* spin */
    }
    auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto &th : ts)
        th.join();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

inline unsigned default_threads()
{
    unsigned n = std::thread::hardware_concurrency();
    return n ? n : 4;
}

int main()
{
    fmt::print("Hello, {}!\n", "world");

    u32 n = 500'000;
    u32 PREFILL = 0;

    using typ = access::Key;

    auto T = default_threads();

    // auto partition = [&](unsigned t, u32 &lo, u32 &hi)
    // {
    //     u32 per = (n + T - 1) / T;
    //     lo = t * per;
    //     hi = std::min<u32>(lo + per, n);
    // };
    auto partition = [&](unsigned t, u32 &lo, u32 &hi)
    {
        u32 measured = n - PREFILL;
        u32 per = (measured + T - 1) / T;
        lo = PREFILL + t * per;
        hi = std::min<u32>(lo + per, n);
    };

    db7::storage::DiskManagerAsync disk_mng_async(".data");
    disk_mng_async.CreateOpenFile(1, 3);
    // disk_mng_async.TruncateFile(2, PAGES);

    db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
    disk_scheduler.Start();

    db7::storage::PageVersionManager version_manager;

    std::vector<typ> strs(n);
    prep_keys<typ>(strs, n);
    std::shuffle(strs.begin(), strs.end(), std::mt19937{42});

    u64 sum_insert = 0;
    u64 sum_get = 0;

    u64 iter = 10;
    for (u32 i = 0; i < iter; i++)
    {
        db7::storage::BufferPool buffer_pool(&disk_scheduler, &version_manager);

        auto btree = db7::access::BTreeIndex<u64>(&buffer_pool, &disk_mng_async, 103);

        // for (u32 i = 0; i < PREFILL; i++)
        // {
        //     auto res = btree.Insert(strs[i], 42);
        //     DB7_ASSERT(res.success, "Failed to insert");
        // }
        double ins_ns = 500;

        for (u32 i = 0; i < n; i++)
        {
            std::cout << i << std::endl;
            auto res = btree.Insert(strs[i], 1000 + i);
            DB7_ASSERT(res.success, "Failed to insert");
            auto v = btree.Get(strs[i]);
            DB7_ASSERT(v.success, "all keys present after concurrent insert");
            DB7_ASSERT(v.value == 1000 + i, "value intact after concurrent insert");
        }

        // double ins_ns = run_parallel(T, [&](unsigned t)
        //                              {
        //     u32 lo, hi;
        //     partition(t, lo, hi);
        //     for (u32 i = lo; i < hi; i++)
        //     {
        //         auto res =  btree.Insert(strs[i], 1000 + i);
        //         DB7_ASSERT(res.success, "Failed to insert");
        //         auto v = btree.Get(strs[i]);
        //         DB7_ASSERT(v.success, "all keys present after concurrent insert");
        //         DB7_ASSERT(v.value == 1000 + i, "value intact after concurrent insert");
        //     } });
        sum_insert += ins_ns;

        // // Phase 1: concurrent disjoint inserts.
        // double ins_ns = run_parallel(T, [&](unsigned t)
        //                              {
        // u32 lo, hi;
        // partition(t, lo, hi);
        // for (u32 i = lo; i < hi; i++)
        // {
        //    auto res =  btree.Insert(strs[i], 1000 + i);
        //    DB7_ASSERT(res.success, "Failed to insert");
        // } });

        // sum_insert += ins_ns;

        double read_ns = 0; // run_parallel(T, [&](unsigned t)
        //                               {
        // u32 lo, hi;
        // partition(t, lo, hi);
        // for (u32 i = lo; i < hi; i++)
        // {
        //    auto v = btree.Get(strs[i]);
        //     DB7_ASSERT(v.success, "all keys present after concurrent insert");
        //     DB7_ASSERT(v.value == 1000 + i, "value intact after concurrent insert");
        // } });

        sum_get += read_ns;

        std::fprintf(stderr, "threads=%u  insert=%.1f ms  read=%.1f ms\n", T,
                     ins_ns / 1e6, read_ns / 1e6);
    }

    std::fprintf(stderr, "Average insert=%.1f ms  read=%.1f ms\n",
                 sum_insert / iter / 1e6, sum_get / iter / 1e6);

    // auto bench = [&](int reps)
    // {
    //     double best = std::numeric_limits<double>::max();
    //     for (int r = 0; r < reps; r++)
    //     {
    //         PagePool pool(80000);

    //         auto btree = BTreeIndex<typ, u64>(&pool);

    //         auto t0 = std::chrono::steady_clock::now();
    //         for (u32 i = 0; i < n; i++)
    //         {
    //             btree.Insert(strs[i], 5);
    //             // btree.Delete(strs[i]);//v.success != false
    //             auto v = btree.Get(strs[i]);
    //             if (v.value != 5)
    //                 throw std::runtime_error("bad");
    //         }
    //         auto t1 = std::chrono::steady_clock::now();

    //         double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    //         if (ns < best)
    //             best = ns;
    //     }
    //     printf("best: %.2f ms  (%.1f ns/op)\n", best / 1e6, best / n);
    // };

    // bench(10);

    return 0;
}