#include <iostream>

#include "common.hpp"
#include <fmt/core.h>
#include <random>
// #include <thread>
// #include <chrono>

#include "catalog/catalog.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"
#include "debug/printer.hpp"
#include "storage/index/btree_index.hpp"

using namespace db7;

static inline u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
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

    alignas(4096) u8 buf[PAGE_SIZE] = {};
    for (u32 i = 0; i < PAGES; i++)
    {
        memset(buf, 0, PAGE_SIZE);
        *reinterpret_cast<u64 *>(buf) = i;
        ssize_t written = pwrite(fd, buf, PAGE_SIZE, (off_t)i * PAGE_SIZE);
        if (written != PAGE_SIZE)
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

int main()
{
    fmt::print("Hello, {}!\n", "world");

    // populate_table();

    db7::storage::DiskManagerAsync disk_mng_async(".data");
    disk_mng_async.CreateOpenFile(1, 3);
    // disk_mng_async.TruncateFile(2, PAGES);

    db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
    disk_scheduler.Start();

    db7::storage::BufferPool buffer_pool(&disk_scheduler);

    db7::storage::BTreeIndex index(&buffer_pool, 1);

    u32 n = 2'000'000;
    std::vector<u32> keys(n);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), std::mt19937{std::random_device{}()});

    u32 num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    u64 t0 = now_ns();
    for (u32 t = 0; t < num_threads; t++)
    {
        threads.emplace_back([&, t]()
                             {
        u32 start = (n * t) / num_threads;
        u32 end = (n * (t + 1)) / num_threads;
        for (u32 i = start; i < end; i++)
        {
            index.Insert(keys[i], i);
        } });
    }
    for (auto &th : threads)
        th.join();
    // for (u32 i = 0; i < n; i++)
    // {
    //     index.Insert(keys[i], i);
    // }

    std::vector<std::thread> read_threads;

    u64 t1 = now_ns();

    for (u32 t = 0; t < num_threads; t++)
    {
        read_threads.emplace_back([&, t]()
                                  {
        u32 start = (n * t) / num_threads;
        u32 end = (n * (t + 1)) / num_threads;
        for (u32 i = start; i < end; i++)
        {
            auto item = index.Get(keys[i]);
            if (item != i)
            {
                throw std::runtime_error("value doesnt match");
            }
        } });
    }
    for (auto &th : read_threads)
        th.join();

    // for (u32 i = 0; i < n; i++)
    // {
    //     auto item = index.Get(keys[i]);
    //     if (item != i)
    //     {
    //         throw std::runtime_error("value doesnt match");
    //     }
    // }
    u64 t2 = now_ns();

    // shared::Print(buffer_pool);

    printf("insert:        %.3f ms\n", (t1 - t0) / 1e6);
    printf("search:        %.3f ms\n", (t2 - t1) / 1e6);

    // auto cat = new catalog::Catalog(&buffer_pool, &disk_mng_async);

    // std::string s = "sssssssssssssssssssssssssssssss";
    // std::span<byte> sdata(reinterpret_cast<byte *>(s.data()), s.size());

    // std::string sa = "aaaa";
    // std::span<byte> sdataa(reinterpret_cast<byte *>(s.data()), s.size());

    // cat->CreateDatabase(nullptr, sdata, true);
    // cat->DeleteDatabase(nullptr, 1);
    // cat->CreateDatabase(nullptr, sdataa, true);

    // std::this_thread::sleep_for(std::chrono::seconds(1));

    disk_scheduler.Stop();

    return 0;
}