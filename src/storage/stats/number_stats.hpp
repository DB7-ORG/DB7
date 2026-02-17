#include "common.hpp"
#include "nullbitmap.hpp"

#include <map>

template <typename T>
struct NumberStats
{
    // TODO think about more helpful stats
    const T *src;
    const ValidityMask *bitmap;
    std::map<T, u32> distinct_values;
    const u32 nitems;
    u32 total_size;
    u32 null_count;
    u32 average_run_len;
    T min;
    T max;
    bool is_sorted;

    NumberStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
        : src(src), bitmap(bitmap), nitems(nitems) {}

    NumberStats() = delete;

    static void GenerateSamples(T *samples, ValidityMask *bitmap)
    {
        // TODO probably dont need intermediate array to avoid copying
    }

    static void GenerateStats(const T *samples, const ValidityMask *bitmap)
    {
    }
};