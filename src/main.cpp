#include <iostream>

#include "common.hpp"
#include <fmt/core.h>

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

    db7::storage::DiskManager disk_mng(".data");

    // disk_mng.CreateTable(1);
    // disk_mng.DropTable(1);

    // void *dest = std::aligned_alloc(4096, 1 << 20);
    // disk_mng.CreateTable(2);
    // disk_mng.ReadPage(2, 2, dest);
    // disk_mng.WritePage(2, 2, dest);
    // disk_mng.ExistsTable(2);
    // disk_mng.TruncateFile(2, 6);
    // disk_mng.ReadPage(2, 4, dest);
    // disk_mng.PageCount(2);

    //////////////////////

    // db7::storage::DiskManagerAsync disk_mng_async();

    // db7::storage::DiskScheduler scheduler(&disk_mng_async, 64);
    // scheduler.Start();

    // // buffer pool submits work
    // db7::storage::IoTask task;
    // task.op = db7::storage::IoTask::READ;
    // task.priority = db7::storage::IoPriority::HIGH;
    // task.fd = fd;
    // task.buf = aligned_buf;
    // task.len = PAGE_SIZE;
    // task.offset = page_id * PAGE_SIZE;
    // task.user_data = nullptr;

    // scheduler.Enqueue(std::move(task));

    auto buffer_pool = new db7::storage::BufferPool(&disk_mng);

    auto cat = new catalog::Catalog(buffer_pool);
    std::string s = "sss";

    cat->CreateDatabase(nullptr, s, true);

    return 0;
}