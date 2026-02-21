#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"
#include "append_valtyp_hmap.hpp"
#include "bit_utils.hpp"
#include "count_hset.hpp"
#include "hyperloglog.hpp"

#include <limits>
#include <vector>
#include <algorithm>
#include <iostream>

template <typename T>
struct SampleStats
{
    std::vector<T> samples;
    const ValidityMask *bitmap;

    u32 size() const { return samples.size(); }

    SampleStats(std::vector<T> samples, const ValidityMask *bitmap)
        : samples(std::move(samples)), bitmap(bitmap)
    {
    }
};

template <typename T>
struct NumberStats
{
    // TODO think about more helpful stats
    const T *src;
    const ValidityMask *bitmap;
    const u32 nitems;
    CountHSet<T> distinct_values;
    u32 bitFreq[33];
    u32 total_size;
    u32 null_count; // TODO useless??
    u32 count_run_len;
    u32 average_run_len;
    T min;
    T max;
    bool is_sorted_asc;
    bool is_sorted_desc;
    // HyperLogLog hll;

    NumberStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
        : src(src), bitmap(bitmap), nitems(nitems), distinct_values(240'000), bitFreq{} //, hll(16) // TODO pick a viable size
    {
        total_size = nitems * sizeof(T);
        is_sorted_asc = true;
        is_sorted_desc = true;
        null_count = 0;
        average_run_len = 0;
        count_run_len = 0;
    }

    NumberStats() = delete;

    inline void ChooseSamplingParams(u32 &sample_target, u32 &num_runs, u32 &run_len, u32 &interval_len)
    {
        if (nitems >= 10'000)
        {
            sample_target = nitems / 100; // 1%
            num_runs = 10;
            run_len = sample_target / num_runs;
            interval_len = nitems / num_runs;
        }
        else if (nitems >= 1'000)
        {
            sample_target = nitems / 10; // 10%
            num_runs = 10;
            run_len = sample_target / num_runs;
            interval_len = nitems / num_runs;
        }
        else
        {
            sample_target = nitems; // 100%
            num_runs = 1;
            run_len = nitems;
            interval_len = nitems;
        }
    }

    void GenerateStats()
    {
        u32 rle_count = 1;
        u32 rle_last_seen = src[0];
        min = rle_last_seen;
        max = rle_last_seen;
        bool allValid = bitmap->AllValid();
        for (u32 i = 1; i < nitems; i++)
        {
            if (!allValid && !bitmap->RowIsValid(i)) // TODO this can be optimize everywhere
            {
                bitFreq[0]++;
                null_count++;
                continue;
            }

            T value = src[i];
            u32 usedBits = CountBitsUsed(value);
            bitFreq[usedBits]++;

            // distinct_values.Inc(value); // TODO Replace this with a map that is using bits instead of bytes
            distinct_values.Push(value);
            // hll.add(value);

            min = std::min(value, min);
            max = std::max(value, max);

            is_sorted_asc &= (rle_last_seen <= value);
            is_sorted_desc &= (rle_last_seen >= value);

            rle_count += (value != rle_last_seen);
            rle_last_seen = value;
        }

        // std::cout << hll.estimate() << std::endl;

        average_run_len = nitems / rle_count;
        count_run_len = rle_count;
    }

    SampleStats<T> GenerateSamples()
    {
        u32 sample_target, num_runs, run_len, interval_len;
        ChooseSamplingParams(sample_target, num_runs, run_len, interval_len);

        std::vector<T> samples(sample_target);

        u32 offset = 0;

        for (u32 i = 0; i < num_runs; i++, offset += interval_len)
        {
            // u32 src_offset = i * interval_len + rand() % (interval_len - run_len);
            // u32 dst_offset = i * run_len;
            // memcpy(samples + dst_offset, src + src_offset, run_len * sizeof(T));
            memcpy(samples.data() + i * run_len, src + offset, run_len * sizeof(T));
        }

        return SampleStats<T>(std::move(samples), nullptr);
    }

    void Print()
    {
        printf("=== Column Stats ===\n");
        printf("  nitems:          %u\n", nitems);
        printf("  total_size:      %u\n", total_size);
        printf("  null_count:      %u\n", null_count);
        printf("  distinct_values: %u\n", distinct_values.Size());
        printf("  average_run_len: %u\n", average_run_len);
        printf("  min:             %s\n", std::to_string(min).c_str());
        printf("  max:             %s\n", std::to_string(max).c_str());
        printf("  sorted_asc:      %s\n", is_sorted_asc ? "yes" : "no");
        printf("  sorted_desc:     %s\n", is_sorted_desc ? "yes" : "no");
        printf("  bit_freq:        ");

        bool first = true;

        for (u32 b = 0; b <= 32; ++b)
        {
            if (bitFreq[b] != 0)
            {
                if (!first)
                    printf(", ");

                printf("%u:%u", b, bitFreq[b]);
                first = false;
            }
        }

        if (first)
            printf("empty");

        printf("\n");
        printf("====================\n");
    }
};