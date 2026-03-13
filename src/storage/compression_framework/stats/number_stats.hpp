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
    u32 total_size;
    u32 bitFreq[MAX_HIST_SIZE];
    u32 count_run_len;
    u32 count_distinct;
    u64 min;
    u64 max;

    NumberStats() : bitFreq{}
    {
        type = StatsType::Number;
        size_of_type = 0;
        num_items = 0;
        total_size = 0;
        count_run_len = 0;
        count_distinct = 0;
        min = UINT64_MAX;
        max = 0;
    }

    NumberStats(
        const u32 *bitFreq,
        u32 num_items,
        u32 total_size,
        u32 count_run_len,
        u32 count_distinct,
        u64 min,
        u64 max,
        u8 size_of_type)
        : total_size(total_size),
          count_run_len(count_run_len),
          count_distinct(count_distinct),
          min(min),
          max(max)
    {
        this->num_items = num_items;
        this->type = StatsType::Number;
        this->size_of_type = size_of_type;
        if (bitFreq != nullptr)
        {
            std::memcpy(this->bitFreq, bitFreq, MAX_HIST_SIZE * sizeof(u32));
        }
        else
        {
            this->bitFreq[0] = UINT32_MAX;
        }
    }

    NumberStats(const NumberStats &other)
    {
        type = StatsType::Number;
        std::memcpy(bitFreq, other.bitFreq, sizeof(bitFreq));
        num_items = other.num_items;
        total_size = other.total_size;
        count_run_len = other.count_run_len;
        count_distinct = other.count_distinct;
        size_of_type = other.size_of_type;
        min = other.min;
        max = other.max;
    }

    NumberStats(const NumberStats *other) : NumberStats(*other) {}

    // TODO fix inserting zero to set
    // TODO early stopping for dict
    template <typename T>
    void GenerateStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
    {
        size_of_type = sizeof(T);
        num_items = nitems;
        total_size = nitems * sizeof(T);
        CountHSet<T> distinct_values(nitems, 2);
        u32 rle_count = 1;
        T rle_last_seen = src[0];
        T lmin = rle_last_seen;
        T lmax = rle_last_seen;
        bool allValid = bitmap->AllValid();

        // u32 used = CountBitsUsed(rle_last_seen);
        // bitFreq[used]++;

        // bool stopDict = false;
        // const u32 half_nitems = nitems / 2;
        for (u32 i = 1; i < nitems; i++)
        {
            if (!allValid && !bitmap->RowIsValid(i)) // TODO this can be optimize everywhere
            {
                bitFreq[0]++;
                continue;
            }

            T value = src[i];
            // u32 usedBits = CountBitsUsed(value);
            // bitFreq[usedBits]++;

            // distinct_values.Inc(value); // TODO Replace this with a map that is using bits instead of bytes

            // if (!stopDict)
            // {
            //     distinct_values.Push(value);
            //     if (i == nitems / 2 && (half_nitems - distinct_values.Size()) * 100 / half_nitems <= 10)
            //     {
            //         stopDict = true;
            //     }
            // }
            distinct_values.Push(value);

            // hll.add(value);

            lmin = std::min(value, lmin);
            lmax = std::max(value, lmax);

            rle_count += (value != rle_last_seen);
            rle_last_seen = value;
        }
        min = ToU64Bits(lmin);
        max = ToU64Bits(lmax);
        // std::cout << hll.estimate() << std::endl;
        count_distinct = distinct_values.Size(); // stopDict ? nitems : distinct_values.Size();
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
        printf("  rle_count:       %u\n", count_run_len);
        printf("  distinct_values: %u\n", count_distinct);
        printf("  min:             %s\n", std::to_string(min).c_str());
        printf("  max:             %s\n", std::to_string(max).c_str());
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