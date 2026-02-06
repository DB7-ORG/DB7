#include <gtest/gtest.h>
#include "../src/storage/compressions/compression.h"
#include <vector>
#include <random>
#include <algorithm>

class FastPForTest : public ::testing::Test
{
protected:
    FastPForEncoder encoder;
    std::vector<u32> input;
    std::vector<u32> encoded;
    std::vector<u32> decoded;

    void SetUp() override
    {
        // Reserve space for worst case
        encoded.resize(10000);
        decoded.resize(10000);
    }

    void EncodeAndDecode(const std::vector<u32> &data)
    {
        input = data;
        encoder.encode(encoded.data(), input.data(), input.size());
        encoder.decode(decoded.data(), encoded.data(), input.size());
        decoded.resize(input.size());
    }
};

TEST_F(FastPForTest, BlockSizeAssertions)
{
    EXPECT_EQ(BlockSize, 256);
    EXPECT_EQ(BlockSize % 256, 0);
}

TEST_F(FastPForTest, EncodeDecodeAllZeros)
{
    std::vector<u32> data(BlockSize, 0);
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeAllOnes)
{
    std::vector<u32> data(BlockSize, 1);
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeSmallValues)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i % 16; // Values 0-15 (4 bits)
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeSequential)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i;
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeMaxValues)
{
    std::vector<u32> data(BlockSize, 0xFFFFFFFF);
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeMixedValues)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = (i % 2 == 0) ? 10 : 1000000;
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeWithExceptions)
{
    std::vector<u32> data(BlockSize, 15); // Most values are small
    data[0] = 100000;                     // Exception
    data[127] = 500000;                   // Exception
    data[255] = 999999;                   // Exception

    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeMultipleBlocks)
{
    std::vector<u32> data(BlockSize * 4); // 4 blocks
    for (size_t i = 0; i < data.size(); i++)
    {
        data[i] = i % 1000;
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeRandomData)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<u32> dist(0, 1000000);

    std::vector<u32> data(BlockSize * 2);
    for (auto &val : data)
    {
        val = dist(rng);
    }

    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeSkewedDistribution)
{
    std::mt19937 rng(123);
    std::vector<u32> data(BlockSize * 3);

    // 90% small values, 10% large values
    for (size_t i = 0; i < data.size(); i++)
    {
        if (i % 10 == 0)
        {
            data[i] = rng() % 10000000; // Large value
        }
        else
        {
            data[i] = rng() % 100; // Small value
        }
    }

    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecode1BitValues)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i % 2; // Only 0 or 1
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodePowerOfTwo)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = 1u << (i % 16); // Powers of 2
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, EncodeDecodeAlternatingPattern)
{
    std::vector<u32> data(BlockSize * 2);
    for (size_t i = 0; i < data.size(); i++)
    {
        data[i] = (i % 4 < 2) ? 5 : 50000;
    }
    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
}

TEST_F(FastPForTest, CompressionRatioSmallValues)
{
    std::vector<u32> data(BlockSize, 7); // 3 bits needed

    u32 encoded_size = encoder.encode(encoded.data(), data.data(), data.size());
    u32 original_size = data.size() * sizeof(u32);

    // Should compress well (exact ratio depends on implementation)
    EXPECT_LT(encoded_size * sizeof(u32), original_size);
}

TEST_F(FastPForTest, VerifyIndividualElements)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i * 7 + 13; // Arbitrary pattern
    }

    EncodeAndDecode(data);

    for (size_t i = 0; i < BlockSize; i++)
    {
        EXPECT_EQ(decoded[i], input[i]) << "Mismatch at index " << i;
    }
}

TEST_F(FastPForTest, RoundTripMultipleTimes)
{
    std::vector<u32> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i % 256;
    }

    // Encode and decode multiple times
    for (int round = 0; round < 5; round++)
    {
        EncodeAndDecode(data);
        EXPECT_EQ(decoded, data) << "Round " << round << " failed";
        data = decoded; // Use decoded as input for next round
    }
}

TEST_F(FastPForTest, EdgeCasesSingleException)
{
    std::vector<u32> data(BlockSize, 1);
    data[BlockSize / 2] = 1000000; // Single outlier

    EncodeAndDecode(data);
    EXPECT_EQ(decoded, input);
    EXPECT_EQ(decoded[BlockSize / 2], 1000000u);
}