#include <iostream>

#include "common.hpp"
#include <fmt/core.h>

#include "catalog/catalog.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "storage/disk_manager/disk_manager.hpp"

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

    db7::storage::DiskManager disk_mng("ssss");

    auto buffer_pool = new db7::storage::BufferPool(&disk_mng);

    auto cat = new catalog::Catalog(buffer_pool);
    std::string s = "sss";

    cat->CreateDatabase(nullptr, s, true);

    return 0;
}