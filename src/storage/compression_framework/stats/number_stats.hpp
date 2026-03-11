#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"
#include "append_valtyp_hmap.hpp"
#include "bit_utils.hpp"
#include "count_hset.hpp"
#include "hyperloglog.hpp"
#include "types.hpp"
#include "bit_utils.hpp"

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

enum UnknownNumberStats
{
    TOTAL_SIZE,
    BIT_FREQ,
    COUNT_RUN_LEN,
    COUNT_DISTINCT,
    MIN,
    MAX,
    _COUNT // always last
};

struct NumberStats : IStats
{
    u32 total_size;
    u32 bitFreq[MAX_HIST_SIZE];
    u32 count_run_len;
    u32 count_distinct;
    u64 min;
    u64 max;
    bool unknown_stats[UnknownNumberStats::_COUNT]; // TODO should be a bitmap

    NumberStats() : bitFreq{}, unknown_stats{}
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

    NumberStats(bool *unknown_stats) : bitFreq{}
    {
        type = StatsType::Number;
        size_of_type = 0;
        num_items = 0;
        total_size = 0;
        count_run_len = 0;
        count_distinct = 0;
        min = UINT64_MAX;
        max = 0;
        std::memcpy(this->unknown_stats, unknown_stats, _COUNT);
    }

    NumberStats(
        const u32 bitFreq[MAX_HIST_SIZE],
        u32 num_items,
        u32 total_size,
        u32 count_run_len,
        u32 count_distinct,
        u64 min,
        u64 max,
        u8 size_of_type,
        bool unknown_stats[UnknownNumberStats::_COUNT])
        : total_size(total_size),
          count_run_len(count_run_len),
          count_distinct(count_distinct),
          min(min),
          max(max),
          unknown_stats(unknown_stats)
    {
        this->num_items = num_items;
        this->type = StatsType::Number;
        this->size_of_type = size_of_type;
        if (bitFreq != nullptr)
        {
            std::memcpy(this->bitFreq, bitFreq, MAX_HIST_SIZE * sizeof(u32));
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
        std::memcpy(unknown_stats, other.unknown_stats, sizeof(unknown_stats));
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
        min = rle_last_seen;
        max = rle_last_seen;
        bool allValid = bitmap->AllValid();
        u32 used = CountBitsUsed(rle_last_seen);
        bitFreq[used]++;
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
            u32 usedBits = CountBitsUsed(value);
            bitFreq[usedBits]++;

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

            min = std::min(u64(value), min);
            max = std::max(u64(value), max);

            rle_count += (value != rle_last_seen);
            rle_last_seen = value;
        }

        // std::cout << hll.estimate() << std::endl;
        count_distinct = distinct_values.Size(); // stopDict ? nitems : distinct_values.Size();
        count_run_len = rle_count;
    }

    template <typename T>
    void GenerateUnknownStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
    {
        size_of_type = sizeof(T);
        num_items = nitems;
        bool allValid = bitmap->AllValid();

        // determine which stats need to be collected
        bool need_total_size = unknown_stats[UnknownNumberStats::TOTAL_SIZE];
        bool need_bit_freq = unknown_stats[UnknownNumberStats::BIT_FREQ];
        bool need_rle = unknown_stats[UnknownNumberStats::COUNT_RUN_LEN];
        bool need_distinct = unknown_stats[UnknownNumberStats::COUNT_DISTINCT];
        bool need_min = unknown_stats[UnknownNumberStats::MIN];
        bool need_max = unknown_stats[UnknownNumberStats::MAX];

        // pre-compute what we can outside the loop
        if (need_total_size)
            total_size = nitems * sizeof(T);

        CountHSet<T> distinct_values(nitems, 2);
        T rle_last_seen = src[0];
        u32 rle_count = 1;

        if (need_min)
            min = rle_last_seen;
        if (need_max)
            max = rle_last_seen;
        if (need_bit_freq)
            bitFreq[CountBitsUsed(rle_last_seen)]++;

        for (u32 i = 1; i < nitems; i++)
        {
            if (!allValid && !bitmap->RowIsValid(i))
            {
                if (need_bit_freq)
                    bitFreq[0]++;
                continue;
            }

            T value = src[i];

            if (need_bit_freq)
            {
                u32 usedBits = CountBitsUsed(value);
                bitFreq[usedBits]++;
            }

            if (need_distinct)
                distinct_values.Push(value);

            if (need_min)
                min = std::min(u64(value), min);
            if (need_max)
                max = std::max(u64(value), max);

            if (need_rle)
            {
                rle_count += (value != rle_last_seen);
                rle_last_seen = value;
            }
        }

        if (need_distinct)
            count_distinct = distinct_values.Size();
        if (need_rle)
            count_run_len = rle_count;

        for (u32 i = 0; i < _COUNT; i++)
        {
            unknown_stats[i] = false;
        }
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

    template <typename T>
    static double CalcError(T real, T predicted)
    {
        double diff = predicted > real
                          ? double(predicted - real)
                          : double(real - predicted);

        return diff / (real + 1);
    }

    static u32 ChangedStats(const NumberStats &real, const NumberStats &predicted, bool *unknown)
    {
        double score = 0.0;
        u32 count = 0;

        if (unknown[MIN])
        {
            score += CalcError(real.min, predicted.min); // TODO for now works on unsigned only
            count++;
        }
        if (unknown[MAX])
        {
            score += CalcError(real.max, predicted.max); // TODO for now works on unsigned only
            count++;
        }
        if (unknown[COUNT_DISTINCT])
        {
            score += CalcError(real.count_distinct, predicted.count_distinct);
            count++;
        }
        if (unknown[COUNT_RUN_LEN])
        {
            score += CalcError(real.count_run_len, predicted.count_run_len);
            count++;
        }
        if (unknown[BIT_FREQ])
        {
            u32 sum = 0;
            for (u32 i = 0; i < MAX_HIST_SIZE; i++)
            {
                double diff = predicted.bitFreq[i] > real.bitFreq[i]
                                  ? double(predicted.bitFreq[i] - real.bitFreq[i])
                                  : double(real.bitFreq[i] - predicted.bitFreq[i]);
                sum += diff;
            }
            score += sum / MAX_HIST_SIZE;
            count++;
        }

        return score / count;
    }

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