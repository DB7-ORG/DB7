#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"
#include "count_hset.hpp"
#include "types.hpp"

struct StringStats : IStats
{
    // TODO think about more helpful stats
    const u8 **src;
    const u32 *lens;
    const ValidityMask *bitmap;
    const u32 nitems;
    u32 total_size;
    u32 null_count; // TODO useless??
    double average_str_len;
    u32 unique_aprox;
    HyperLogLog hll;

    StringStats(const u8 **src, const u32 *lens, const ValidityMask *bitmap, const u32 nitems)
        : src(src), lens(lens), bitmap(bitmap), nitems(nitems), hll(16)
    {
        total_size = lens[nitems - 1];
        average_str_len = 0.0;
        unique_aprox = 0;
        null_count = 0;
    }

    StringStats() = delete;

    // void GenerateSamples(u8 **samples, ValidityMask *bitmap)
    // {
    //     // TODO probably dont need intermediate array to avoid copying
    // }

    // TODO this is not handling a case where i have really large string that is repeaded many times
    // and rest of small strings. This can be problematic when doing dictionary encoding and trying
    // to estimate the size.
    // One way to handle this to build a hash map where we only compare 64 bit hash values and act like there
    // is no different values w same hash to avoid memcpy which should be as fast as integer statistics.
    // Then later calculate EV (might be better approaches)
    void GenerateStats()
    {
        bool allValid = bitmap->AllValid();
        u32 total_len = 0;
        for (u32 i = 0; i < nitems; i++)
        {
            if (!allValid && !bitmap->RowIsValid(i)) // TODO this can be optimize everywhere
            {
                null_count++;
                continue;
            }

            const u8 *value = src[i];
            const u32 len = lens[i + 1] - lens[i];

            total_len += len;
            hll.add(value, len);
        }

        average_str_len = (double)total_len / nitems;
        unique_aprox = hll.estimate();
    }

    void Print() const
    {
        printf("=== Column Stats ===\n");
        printf("  total_size:      %u\n", total_size);
        printf("  null_count:      %u\n", null_count);
        printf("  average_str_len: %f\n", average_str_len);
        printf("  unique_aprox:    %u\n", unique_aprox);
    }
};