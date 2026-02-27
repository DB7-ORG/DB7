#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"
#include "append_valtyp_hmap.hpp"
#include "bit_utils.hpp"
#include "count_hset.hpp"
#include "hyperloglog.hpp"
#include "types.hpp"

#include <limits>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>

constexpr u32 FREQ_SIZE = 65;

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

struct NumberStats : IStats
{
    u32 bitFreq[FREQ_SIZE];
    u32 uniqueBitFreq[FREQ_SIZE];
    u32 num_items;
    u32 total_size;
    u32 null_count; // TODO useless??
    u32 count_run_len;
    u32 average_run_len;
    u32 count_distinct;
    bool is_sorted_asc;
    bool is_sorted_desc;
    u8 size_of_type;

    // MinMax min;
    // MinMax max;

    // HyperLogLog hll;

    NumberStats() : bitFreq{}, uniqueBitFreq{} //, hll(16) // TODO pick a viable size
    {
        type = StatsType::Number;
        num_items = 0;
        total_size = 0;
        is_sorted_asc = true;
        is_sorted_desc = true;
        null_count = 0;
        average_run_len = 0;
        count_run_len = 0;
        count_distinct = 0;
        size_of_type = 0;
    }

    NumberStats(
        const u32 bitFreq[FREQ_SIZE],
        const u32 uniqueBitFreq[FREQ_SIZE],
        u32 num_items,
        u32 total_size,
        u32 null_count,
        u32 count_run_len,
        u32 average_run_len,
        u32 count_distinct,
        bool is_sorted_asc,
        bool is_sorted_desc,
        u8 size_of_type)
        : num_items(num_items),
          total_size(total_size),
          null_count(null_count),
          count_run_len(count_run_len),
          average_run_len(average_run_len),
          count_distinct(count_distinct),
          is_sorted_asc(is_sorted_asc),
          is_sorted_desc(is_sorted_desc),
          size_of_type(size_of_type)
    {
        this->type = StatsType::Number;
        std::memcpy(this->bitFreq, bitFreq, FREQ_SIZE * sizeof(u32));
        std::memcpy(this->uniqueBitFreq, uniqueBitFreq, FREQ_SIZE * sizeof(u32));
    }

    NumberStats(const NumberStats &other)
    {
        type = StatsType::Number;
        std::memcpy(bitFreq, other.bitFreq, sizeof(bitFreq));
        std::memcpy(uniqueBitFreq, other.uniqueBitFreq, sizeof(uniqueBitFreq));
        num_items = other.num_items;
        total_size = other.total_size;
        null_count = other.null_count;
        count_run_len = other.count_run_len;
        average_run_len = other.average_run_len;
        count_distinct = other.count_distinct;
        is_sorted_asc = other.is_sorted_asc;
        is_sorted_desc = other.is_sorted_desc;
        size_of_type = other.size_of_type;

        // min = other.min;
        // max = other.max;
    }

    NumberStats(const NumberStats *other) : NumberStats(*other) {}

    template <typename T>
    void GenerateStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
    {
        size_of_type = sizeof(T);
        num_items = nitems;
        total_size = nitems * sizeof(T);
        CountHSet<T> distinct_values(2 * nitems);
        u32 rle_count = 1;
        T rle_last_seen = src[0];
        // min = rle_last_seen;
        // max = rle_last_seen;
        bool allValid = bitmap->AllValid();
        u32 used = CountBitsUsed(rle_last_seen);
        bitFreq[used]++;
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
            uniqueBitFreq[usedBits] += distinct_values.Push(value);
            // hll.add(value);

            // min = std::min(value, min);
            // max = std::max(value, max);

            is_sorted_asc &= (rle_last_seen <= value);
            is_sorted_desc &= (rle_last_seen >= value);

            rle_count += (value != rle_last_seen);
            rle_last_seen = value;
        }

        // std::cout << hll.estimate() << std::endl;
        count_distinct = distinct_values.Size();
        average_run_len = nitems / rle_count;
        count_run_len = rle_count;
    }

    // inline void ChooseSamplingParams(u32 &sample_target, u32 &num_runs, u32 &run_len, u32 &interval_len)
    // {
    //     if (nitems >= 10'000)
    //     {
    //         sample_target = nitems / 100; // 1%
    //         num_runs = 10;
    //         run_len = sample_target / num_runs;
    //         interval_len = nitems / num_runs;
    //     }
    //     else if (nitems >= 1'000)
    //     {
    //         sample_target = nitems / 10; // 10%
    //         num_runs = 10;
    //         run_len = sample_target / num_runs;
    //         interval_len = nitems / num_runs;
    //     }
    //     else
    //     {
    //         sample_target = nitems; // 100%
    //         num_runs = 1;
    //         run_len = nitems;
    //         interval_len = nitems;
    //     }
    // }

    // SampleStats<T> GenerateSamples()
    // {
    //     u32 sample_target, num_runs, run_len, interval_len;
    //     ChooseSamplingParams(sample_target, num_runs, run_len, interval_len);

    //     std::vector<T> samples(sample_target);

    //     u32 offset = 0;

    //     for (u32 i = 0; i < num_runs; i++, offset += interval_len)
    //     {
    //         // u32 src_offset = i * interval_len + rand() % (interval_len - run_len);
    //         // u32 dst_offset = i * run_len;
    //         // memcpy(samples + dst_offset, src + src_offset, run_len * sizeof(T));
    //         memcpy(samples.data() + i * run_len, src + offset, run_len * sizeof(T));
    //     }

    //     return SampleStats<T>(std::move(samples), nullptr);
    // }

    void Print()
    {
        printf("=== Column Stats ===\n");
        printf("  nitems:          %u\n", num_items);
        printf("  total_size:      %u\n", total_size);
        printf("  null_count:      %u\n", null_count);
        printf("  distinct_values: %u\n", count_distinct);
        printf("  average_run_len: %u\n", average_run_len);
        // printf("  min:             %s\n", std::to_string(min).c_str());
        // printf("  max:             %s\n", std::to_string(max).c_str());
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