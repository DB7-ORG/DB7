#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include "../src/storage/compressions/compression.hpp"

// Test fixture for BitPack tests
class BitPackSingleTest : public ::testing::TestWithParam<uint32_t>
{
protected:
    uint32_t bits;
    uint32_t mask;
    static constexpr uint32_t MAX_ITEMS = 512;
    static constexpr uint32_t MAX_COMPRESSED_SIZE = 64 * 8;

    uint32_t values[MAX_ITEMS];
    uint32_t compressed[MAX_COMPRESSED_SIZE];

    void SetUp() override
    {
        bits = GetParam();
        mask = (1U << bits) - 1;
        std::memset(values, 0, sizeof(values));
        std::memset(compressed, 0, sizeof(compressed));
    }

    uint32_t decode_single(uint32_t idx)
    {
        return BitPackEncoder<u32>::DecodeSingle(compressed, idx, bits);
    }

    void Encode(uint32_t nitems)
    {
        BitPackEncoder<u32>::Encode(compressed, values, nitems, bits);
    }
};

// Test sequential values (0, 1, 2, 3, ...)
TEST_P(BitPackSingleTest, SequentialValues)
{
    constexpr uint32_t nitems = 256;

    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = i & mask;
    }

    Encode(nitems);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = decode_single(i);
        uint32_t expected = i & mask;
        EXPECT_EQ(decoded, expected)
            << "Failed at index " << i << " for bits=" << bits;
    }
}

// Test all maximum values
TEST_P(BitPackSingleTest, MaxValues)
{
    constexpr uint32_t nitems = 256;

    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = mask; // all bits set
    }

    Encode(nitems);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = decode_single(i);
        EXPECT_EQ(decoded, mask)
            << "Failed at index " << i << " for bits=" << bits;
    }
}

// Test alternating pattern (0, max, 0, max, ...)
TEST_P(BitPackSingleTest, AlternatingPattern)
{
    constexpr uint32_t nitems = 256;

    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = (i % 2 == 0) ? 0 : mask;
    }

    Encode(nitems);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t expected = (i % 2 == 0) ? 0 : mask;
        uint32_t decoded = decode_single(i);
        EXPECT_EQ(decoded, expected)
            << "Failed at index " << i << " for bits=" << bits;
    }
}

// Test values spanning word boundaries
TEST_P(BitPackSingleTest, SpanningBoundaries)
{
    constexpr uint32_t nitems = 256;

    // Fill with pseudo-random pattern
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = (i * 7 + 3) & mask;
    }

    Encode(nitems);

    // Check specifically around word boundaries
    for (uint32_t lane = 0; lane < 32; lane++)
    {
        uint32_t bitpos = lane * bits;
        uint32_t shift = bitpos & 31;

        if (shift > 32 - bits)
        {
            // This lane spans a word boundary
            for (uint32_t pos = 0; pos < 8; pos++)
            {
                uint32_t idx = lane * 8 + pos;
                uint32_t decoded = decode_single(idx);
                uint32_t expected = values[idx];
                EXPECT_EQ(decoded, expected)
                    << "Failed at index " << idx
                    << " (lane=" << lane << ", shift=" << shift << ")"
                    << " for bits=" << bits;
            }
        }
    }
}

// Test encoding/decoding multiple blocks
TEST_P(BitPackSingleTest, MultipleBlocks)
{
    constexpr uint32_t nitems = 512;

    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = i & mask;
    }

    Encode(nitems);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = decode_single(i);
        uint32_t expected = i & mask;
        EXPECT_EQ(decoded, expected)
            << "Failed at index " << i << " for bits=" << bits;
    }
}

// Instantiate tests for bits 1-31
INSTANTIATE_TEST_SUITE_P(
    BitPackSingleTests,
    BitPackSingleTest,
    ::testing::Range(1u, 32u),
    [](const ::testing::TestParamInfo<uint32_t> &info)
    {
        return "Bits_" + std::to_string(info.param);
    });

// TODO move main outside to seperate folder
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}