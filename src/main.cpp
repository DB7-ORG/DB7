#include <iostream>

#include "common.hpp"
#include <fmt/core.h>

#include "catalog/catalog.hpp"

static inline u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

int main()
{
    fmt::print("Hello, {}!\n", "world");

    db7::catalog::Catalog cat;
    std::string s = "sss";
    cat.CreateDatabase(s);

    return 0;
}