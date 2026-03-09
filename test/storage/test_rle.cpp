#include <gtest/gtest.h>
#include "../src/storage/compressions/compression.hpp"

// Helper to allocate RleEncodedRes buffers
template <typename T>
RleEncodedRes<T> AllocEncoded(u32 maxRuns)
{
    RleEncodedRes<T> res;
    res.values = new T[maxRuns];
    res.counts = new u16[maxRuns];
    res.count = 0;
    return res;
}

template <typename T>
void FreeEncoded(RleEncodedRes<T> &res)
{
    delete[] res.values;
    delete[] res.counts;
}

TEST(RleEncode, SingleElement)
{
    ValidityMask nullmap;
    u32 in[] = {42};
    auto out = AllocEncoded<u32>(1);

    RleEncoder::Encode(&out, in, &nullmap, 1);

    EXPECT_EQ(out.count, 1);
    EXPECT_EQ(out.values[0], 42u);
    EXPECT_EQ(out.counts[0], 1);

    FreeEncoded(out);
}

TEST(RleDecode, SingleElement)
{
    RleEncodedRes<u32> in;
    u32 values[] = {42};
    u16 counts[] = {1};
    in.values = values;
    in.counts = counts;
    in.count = 1;

    // +8 extra for the 256-bit overwrite safety margin
    u32 out[1 + 8] = {};
    RleEncoder::Decode(out, &in);

    EXPECT_EQ(out[0], 42u);
}

TEST(RleEncode, OverflowSplitsRun)
{
    ValidityMask nullmap;
    const u32 nitems = (u32)UINT16_MAX + 1;
    u32 *in = new u32[nitems];
    std::fill(in, in + nitems, 99u);

    auto out = AllocEncoded<u32>(2); // must split into 2 runs

    RleEncoder::Encode(&out, in, &nullmap, nitems);

    EXPECT_EQ(out.count, 2);
    EXPECT_EQ(out.values[0], 99u);
    EXPECT_EQ(out.counts[0], UINT16_MAX);
    EXPECT_EQ(out.values[1], 99u);
    EXPECT_EQ(out.counts[1], 1);

    delete[] in;
    FreeEncoded(out);
}

TEST(RleDecode, OverflowRoundtrip)
{
    const u32 nitems = (u32)UINT16_MAX + 1;
    RleEncodedRes<u32> in;
    u32 values[] = {99, 99};
    u16 counts[] = {UINT16_MAX, 1};
    in.values = values;
    in.counts = counts;
    in.count = 2;

    u32 *out = new u32[nitems + 8]();
    RleEncoder::Decode(out, &in);

    for (u32 i = 0; i < nitems; i++)
        EXPECT_EQ(out[i], 99u) << "at index " << i;

    delete[] out;
}

TEST(RleEncode, VariableRuns)
{
    constexpr u32 NITEMS = 20000;
    ValidityMask nullmap;

    // Build input: runs of varying lengths cycling through values 1-10
    u32 in[NITEMS];
    // expected runs: (value, count) pairs
    struct Run
    {
        u32 value;
        u16 count;
    };
    std::vector<Run> expectedRuns;

    u32 runLengths[] = {1, 5, 3, 100, 7, 50, 2, 200, 1, 10};
    u32 runValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    constexpr u32 PATTERN_SIZE = 10;

    u32 pos = 0;
    u32 patIdx = 0;
    while (pos < NITEMS)
    {
        u32 val = runValues[patIdx % PATTERN_SIZE];
        u32 len = std::min((u32)runLengths[patIdx % PATTERN_SIZE], NITEMS - pos);
        std::fill(in + pos, in + pos + len, val);
        expectedRuns.push_back({val, (u16)len});
        pos += len;
        patIdx++;
    }

    auto out = AllocEncoded<u32>(NITEMS);
    RleEncoder::Encode(&out, in, &nullmap, NITEMS);

    EXPECT_EQ(out.count, expectedRuns.size());
    for (u32 i = 0; i < out.count; i++)
    {
        EXPECT_EQ(out.values[i], expectedRuns[i].value) << "at run " << i;
        EXPECT_EQ(out.counts[i], expectedRuns[i].count) << "at run " << i;
    }

    FreeEncoded(out);
}

TEST(RleDecode, VariableRuns)
{
    constexpr u32 NITEMS = 20000;

    struct Run
    {
        u32 value;
        u16 count;
    };
    u32 runLengths[] = {1, 5, 3, 100, 7, 50, 2, 200, 1, 10};
    u32 runValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    constexpr u32 PATTERN_SIZE = 10;

    std::vector<u32> encodedValues;
    std::vector<u16> encodedCounts;
    std::vector<u32> expected;

    u32 pos = 0;
    u32 patIdx = 0;
    while (pos < NITEMS)
    {
        u32 val = runValues[patIdx % PATTERN_SIZE];
        u32 len = std::min((u32)runLengths[patIdx % PATTERN_SIZE], NITEMS - pos);
        encodedValues.push_back(val);
        encodedCounts.push_back((u16)len);
        for (u32 i = 0; i < len; i++)
            expected.push_back(val);
        pos += len;
        patIdx++;
    }

    RleEncodedRes<u32> in;
    in.values = encodedValues.data();
    in.counts = encodedCounts.data();
    in.count = encodedValues.size();

    std::vector<u32> out(NITEMS + 8, 0);
    RleEncoder::Decode(out.data(), &in);

    for (u32 i = 0; i < NITEMS; i++)
        EXPECT_EQ(out[i], expected[i]) << "at index " << i;
}