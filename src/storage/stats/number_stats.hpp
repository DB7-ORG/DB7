#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"

#include "append_valtyp_hmap.hpp"

template <typename T>
struct NumberStats
{
    // TODO think about more helpful stats
    const T *src;
    const ValidityMask *bitmap;
    AppendOnlyHMap<T> distinct_values;
    const u32 nitems;
    u32 total_size;
    u32 null_count;
    u32 average_run_len;
    T min;
    T max;
    bool is_sorted_asc;
    bool is_sorted_desc;

    NumberStats(const T *src, const ValidityMask *bitmap, const u32 nitems)
        : src(src), bitmap(bitmap), distinct_values(400'000), nitems(nitems) // TODO pick a viable size
    {
        total_size = nitems * sizeof(T);
        is_sorted_asc = true;
        is_sorted_desc = true;
        null_count = 0;
        min = std::numeric_limits<T>::max();
        max = std::numeric_limits<T>::lowest();
        average_run_len = 0;
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

    std::vector<T> GenerateSamples()
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

        return samples;
    }

    void GenerateStats()
    {
        u32 rle_count = 1;
        u32 rle_last_seen = src[0];
        bool allValid = bitmap->AllValid();
        for (u32 i = 1; i < nitems; i++)
        {
            if (!allValid && !bitmap->RowIsValid(i)) // TODO this can be optimize everywhere
            {
                null_count++;
                continue;
            }

            T value = src[i];

            distinct_values.Inc(value);

            min = std::min(value, min);
            max = std::max(value, max);

            is_sorted_asc &= (rle_last_seen <= value);
            is_sorted_desc &= (rle_last_seen >= value);

            rle_count += (value != rle_last_seen);
            rle_last_seen = value;
        }
        average_run_len = nitems / rle_count;
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
        printf("====================\n");
    }
};