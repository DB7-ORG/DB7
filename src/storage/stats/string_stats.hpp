#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"

#include <set>

struct StringStats
{
    // TODO think about more helpful stats
    const u8 **src;
    const ValidityMask *bitmap;
    std::set<std::string_view> distinct_values;
    const u32 nitems;
    u32 total_size;
    u32 null_count;
    u32 average_len;
    bool is_sorted;

    StringStats(const u8 **src, const ValidityMask *bitmap, const u32 nitems)
        : src(src), bitmap(bitmap), nitems(nitems) {}

    StringStats() = delete;

    void GenerateSamples(u8 **samples, ValidityMask *bitmap)
    {
        // TODO probably dont need intermediate array to avoid copying
    }

    void GenerateStats(const u8 **samples, const ValidityMask *bitmap)
    {
    }
};