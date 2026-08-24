#include <iostream>

#include "common.hpp"
#include <fmt/core.h>
#include <random>
#include <barrier>
#include <algorithm>
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
#include "access/data_chunk.hpp"
#include "catalog/builder.hpp"
#include "access/index_schema.hpp"

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

db7::storage::BufferPool *g_buffer_pool;
db7::catalog::Catalog *g_catalog;

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

constexpr auto phys_type = access::type_id::VARCHAR;

template <typename T>
void prep_keys(std::vector<access::DataChunk *> &strs, u32 n)
{

    auto layout = access::DataChunkLayout({1}, {access::SizeOf(phys_type)});
    std::vector<access::TypeSize> types{{1, phys_type}};
    for (int i = 0; i < int(n); i++)
    {
        auto chunk = layout.CreateDataChunk();
        for (int j = 0; j < int(types.size()); j++)
        {
            char num[11];
            std::snprintf(num, sizeof(num), "%05u", i); // zero-padded: sorts correctly
            const std::string s = "k⎞" + std::string(num);
            db7::storage::VarlenEntry entry;
            entry.Set(std::span<const char>(s.data(), s.size()));
            std::memcpy(chunk->Access(0), &entry, access::SizeOf(phys_type));
        }
        strs[i] = chunk;
    }
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

// int main()
// {
//     fmt::print("Hello, {}!\n", "world");

//     u32 n = 500'000;
//     u32 PREFILL = 0;

//     using typ = access::DataChunk *;

//     auto T = default_threads();

//     auto partition = [&](unsigned t, u32 &lo, u32 &hi)
//     {
//         u32 measured = n - PREFILL;
//         u32 per = (measured + T - 1) / T;
//         lo = PREFILL + t * per;
//         hi = std::min<u32>(lo + per, n);
//     };

//     db7::storage::DiskManagerAsync disk_mng_async(".data");
//     disk_mng_async.CreateOpenFile(1, 3);

//     db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
//     disk_scheduler.Start();

//     db7::storage::PageVersionManager version_manager;

//     std::vector<typ> strs(n);
//     prep_keys<typ>(strs, n);
//     std::shuffle(strs.begin(), strs.end(), std::mt19937{42});

//     u64 sum_insert = 0;
//     u64 sum_get = 0;

//     u64 iter = 40;
//     for (u32 i = 0; i < iter; i++)
//     {
//         db7::storage::BufferPool buffer_pool(&disk_scheduler, &version_manager);

//         auto btree = db7::access::BTreeIndex<TupleId>(&buffer_pool, &disk_mng_async, 103, {{1, phys_type}});

//         for (u32 i = 0; i < PREFILL; i++)
//         {
//             auto res = btree.Insert(strs[i], 1000 + i);
//             DB7_ASSERT(res.success, "Failed to insert");
//             auto res_vec = shared::VectorValues<TupleId>();
//             auto v = btree.Get(strs[i], res_vec);
//             DB7_ASSERT(v.success, "all keys present after concurrent insert");
//             DB7_ASSERT(std::ranges::find(res_vec.vec, TupleId{1000 + i}) != res_vec.vec.end(),
//                        "value 1000+i present after concurrent insert");
//         }
//         // double ins_ns = 500;

//         double ins_ns = run_parallel(T, [&](unsigned t)
//                                      {
//             u32 lo, hi;
//             partition(t, lo, hi);
//             for (u32 i = lo; i < hi; i++)
//             {
//                 auto res = btree.Insert(strs[i], 1000 + i);
//                 DB7_ASSERT(res.success, "Failed to insert");
//                 auto res_vec = shared::VectorValues<TupleId>();
//                 auto v = btree.Get(strs[i], res_vec);
//                 DB7_ASSERT(v.success, "all keys present after concurrent insert");
//                 DB7_ASSERT(std::ranges::find(res_vec.vec, TupleId{1000 + i}) != res_vec.vec.end(),
//                         "value 1000+i present after concurrent insert");
//             } });
//         sum_insert += ins_ns;

//         double read_ns = 0;
//         // double read_ns = run_parallel(T, [&](unsigned t)
//         //                               {
//         //     u32 lo, hi;
//         //     partition(t, lo, hi);
//         //     for (u32 i = lo; i < hi; i++)
//         //     {
//         //         auto res_vec = access::VectorValues<u64>();
//         //         auto v = btree.Get(strs[i], res_vec);
//         //         DB7_ASSERT(v.success, "all keys present after concurrent insert");
//         //         DB7_ASSERT(std::ranges::find(res_vec.vec, 1000 + i) != res_vec.vec.end(),
//         //             "value 1000+i present after concurrent insert");
//         //     } });

//         sum_get += read_ns;

//         std::fprintf(stderr, "threads=%u  insert=%.1f ms  read=%.1f ms\n", T,
//                      ins_ns / 1e6, read_ns / 1e6);
//     }

//     std::fprintf(stderr, "Average insert=%.1f ms  read=%.1f ms\n",
//                  sum_insert / iter / 1e6, sum_get / iter / 1e6);

//     return 0;
// }

int main()
{
    fmt::print("Hello, {}!\n", "world");

    // // populate_table();

    db7::storage::DiskManagerAsync disk_mng_async(".data");

    db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
    disk_scheduler.Start();

    db7::storage::PageVersionManager version_manager;

    db7::storage::BufferPool buffer_pool(&disk_scheduler, &version_manager);
    g_buffer_pool = &buffer_pool;

    db7::transaction::TimestampManager timestamp_manager;

    db7::shared::ObjectPool<shared::FixedBumpArena> pool(10'000, 2000);

    db7::transaction::TransactionManager txn_manager(&timestamp_manager, &buffer_pool, &version_manager, &pool);

    auto cat = new catalog::Catalog(&buffer_pool, &disk_mng_async);
    g_catalog = cat;

    db7::transaction::TransactionContext *context = txn_manager.BeginTransaction();

    db7::transaction::TransactionContext *context1 = txn_manager.BeginTransaction();

    std::string s = "ssss";
    std::span<byte> sdata(reinterpret_cast<byte *>(s.data()), s.size());

    std::string s1 = "Jovo";
    std::span<byte> sdata1(reinterpret_cast<byte *>(s1.data()), s1.size());

    std::string s2 = "Jovo2";
    std::span<byte> sdata2(reinterpret_cast<byte *>(s2.data()), s2.size());

    std::string s3 = "ssss33";
    std::span<byte> sdata3(reinterpret_cast<byte *>(s3.data()), s3.size());

    std::string s4 = "ssss44";
    std::span<byte> sdata4(reinterpret_cast<byte *>(s4.data()), s4.size());

    auto db_oid = cat->CreateDatabase(context, sdata, true);
    DB7_ASSERT(db_oid.success, "Failed database");

    auto *db_catalog = cat->GetDatabaseCatalog(db_oid.value);

    auto resns = db_catalog->CreateNamespace(context, sdata1);
    DB7_ASSERT(resns.success, "Failed namespace");

    auto schema = catalog::Builder::CreateTypeSchema();
    auto table_res = db_catalog->CreateTable(context, sdata2, resns.value, schema);
    DB7_ASSERT(table_res.success, "Failed table");

    std::vector<access::SchemaColumn> columns;
    columns.reserve(1);
    columns.emplace_back(7000, access::type_id::INTEGER, "typlen");
    access::IndexSchema idx_schema(std::move(columns), false, false, false, false);
    auto idx_res = db_catalog->CreateIndex(context, sdata3, table_res.value, resns.value, idx_schema);
    DB7_ASSERT(idx_res.success, "Failed index");

    catalog::ConstraintProps props = {
        sdata4,
        resns.value,
        catalog::ConType::PRIMARY_KEY,
        false,
        false,
        false,
        table_res.value,
        idx_res.value,
        table_res.value};
    auto rel_res = db_catalog->CreateConstraint(context, props);
    DB7_ASSERT(rel_res.success, "Failed namespace");

    db_catalog->Select(context, 0);
    db_catalog->Select(context, 1);
    db_catalog->Select(context, 2);
    db_catalog->Select(context, 3);
    db_catalog->Select(context, 4);

    txn_manager.Commit(context);

    txn_manager.Commit(context1);

    delete cat;

    disk_scheduler.Stop();

    return 0;
}