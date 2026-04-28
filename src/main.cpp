#include <iostream>

#include "common.hpp"
#include <fmt/core.h>
#include <random>

#include "catalog/catalog.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager.hpp"
#include "storage/disk_manager/disk_scheduler.hpp"

using namespace db7;

static inline u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

int main()
{
    fmt::print("Hello, {}!\n", "world");

    db7::storage::DiskManagerAsync disk_mng_async(".data");
    disk_mng_async.CreateTable(2);
    disk_mng_async.TruncateFile(2, 500);

    auto len = 1 << 20;
    u8 *dest = (u8 *)std::aligned_alloc(4096, len);

    db7::storage::DiskScheduler disk_scheduler(&disk_mng_async);
    disk_scheduler.Start();

    db7::storage::BufferPool buffer_pool(&disk_scheduler);

    u64 t00 = now_ns();

    dest[0] = 'a';
    dest[1] = 't';

    u64 t0 = now_ns();

    u64 t1 = now_ns();

    u64 t2 = now_ns();

    fmt::print("{} {}\n", (char)dest[0], (char)dest[1]);

    printf("init queue:        %.3f ms\n", (t0 - t00) / 1e6);
    printf("write:        %.3f ms\n", (t1 - t0) / 1e6);
    printf("read:        %.3f ms\n", (t2 - t1) / 1e6);

    auto cat = new catalog::Catalog(&buffer_pool);

    std::string s = "sss";

    cat->CreateDatabase(nullptr, s, true);

    return 0;
}