#include <gtest/gtest.h>
#include "../src/storage/compressions/compression.hpp"
#include <vector>
#include <random>
#include <algorithm>

template <typename T>
class FastPForTest : public ::testing::Test
{
protected:
    FastPForEncoder encoder;
    std::vector<T> input;
    std::vector<u32> encoded;
    std::vector<T> decoded;

    void SetUp() override
    {
        // Reserve space for worst case
        encoded.resize(10000);
        decoded.resize(10000);
    }

    void EncodeAndDecode(const std::vector<T> &data)
    {
        input = data;
        encoder.Encode(encoded.data(), input.data(), input.size());
        encoder.Decode(decoded.data(), encoded.data(), input.size());
        decoded.resize(input.size());
    }
};

using TestTypes = ::testing::Types<u16, u32, u64>;
TYPED_TEST_SUITE(FastPForTest, TestTypes);

TYPED_TEST(FastPForTest, BlockSizeAssertions)
{
    EXPECT_EQ(BlockSize, 256);
    EXPECT_EQ(BlockSize % 256, 0);
}

TYPED_TEST(FastPForTest, EncodeDecodeAllZeros)
{
    std::vector<TypeParam> data(BlockSize, 0);
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeAllOnes)
{
    std::vector<TypeParam> data(BlockSize, 1);
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeSmallValues)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i % 16; // Values 0-15 (4 bits)
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeSequential)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i;
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeMaxValues)
{
    std::vector<TypeParam> data(BlockSize, 5000);
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeMixedValues)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = (i % 2 == 0) ? 10 : 1000;
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeWithExceptions)
{
    std::vector<TypeParam> data(BlockSize, 15); // Most values are small
    data[0] = 1000;                             // Exception
    data[127] = 5000;                           // Exception
    data[255] = 9999;                           // Exception

    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeMultipleBlocks)
{
    std::vector<TypeParam> data(BlockSize * 4); // 4 blocks
    for (size_t i = 0; i < data.size(); i++)
    {
        data[i] = i % 1000;
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeRandomData)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<u32> dist(0, 1000);

    std::vector<TypeParam> data(BlockSize * 2);
    for (auto &val : data)
    {
        val = dist(rng);
    }

    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeSkewedDistribution)
{
    std::mt19937 rng(123);
    std::vector<TypeParam> data(BlockSize * 3);

    // 90% small values, 10% large values
    for (size_t i = 0; i < data.size(); i++)
    {
        if (i % 10 == 0)
        {
            data[i] = rng() % 10000; // Large value
        }
        else
        {
            data[i] = rng() % 100; // Small value
        }
    }

    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecode1BitValues)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i % 2; // Only 0 or 1
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodePowerOfTwo)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = 1u << (i % 16); // Powers of 2
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, EncodeDecodeAlternatingPattern)
{
    std::vector<TypeParam> data(BlockSize * 2);
    for (size_t i = 0; i < data.size(); i++)
    {
        data[i] = (i % 4 < 2) ? 5 : 5000;
    }
    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
}

TYPED_TEST(FastPForTest, CompressionRatioSmallValues)
{
    std::vector<TypeParam> data(BlockSize, 7); // 3 bits needed

    u32 encoded_size = this->encoder.Encode(this->encoded.data(), data.data(), data.size());
    u32 original_size = data.size() * sizeof(TypeParam);

    // Should compress well (exact ratio depends on implementation)
    EXPECT_LT(encoded_size * sizeof(TypeParam), original_size);
}

TYPED_TEST(FastPForTest, VerifyIndividualElements)
{
    std::vector<TypeParam> data(BlockSize);
    for (size_t i = 0; i < BlockSize; i++)
    {
        data[i] = i * 7 + 13; // Arbitrary pattern
    }

    this->EncodeAndDecode(data);

    for (size_t i = 0; i < BlockSize; i++)
    {
        EXPECT_EQ(this->decoded[i], this->input[i]) << "Mismatch at index " << i;
    }
}

TYPED_TEST(FastPForTest, EdgeCasesSingleException)
{
    std::vector<TypeParam> data(BlockSize, 1);

    data[BlockSize / 2] = 6000; // Single outlier

    this->EncodeAndDecode(data);
    EXPECT_EQ(this->decoded, this->input);
    EXPECT_EQ(this->decoded[BlockSize / 2], 6000);
}