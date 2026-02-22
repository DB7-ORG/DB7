#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include "../src/storage/compressions/compression.hpp"

// Type aliases to match your code
using u64 = uint64_t;
using u32 = uint32_t;

// Test fixture
class BitPackEncoderTest : public ::testing::Test
{
protected:
    BitPackEncoder<u32> encoder;
    std::vector<u32> input;
    std::vector<u64> encoded;
    std::vector<u32> decoded;

    void SetUp() override
    {
        input.clear();
        encoded.clear();
        decoded.clear();
    }

    void TestRoundTrip(const std::vector<u32> &values, u32 usedBits)
    {
        input = values;

        // Calculate maximum encoded size
        size_t encodedSize = ((values.size() * usedBits + 63) / 64) + 1;
        encoded.resize(encodedSize, 0);
        decoded.resize(values.size(), 0);

        // Encode
        u64 *encEnd = encoder.ScalarEncode(encoded.data(), input.data(),
                                           input.size(), usedBits);
        (void)encEnd;
        // Decode
        u32 *decEnd = encoder.ScalarDecode(decoded.data(), encoded.data(),
                                           input.size(), usedBits);

        // Verify
        u32 mask = (1ULL << usedBits) - 1;
        for (size_t i = 0; i < values.size(); i++)
        {
            EXPECT_EQ(decoded[i], values[i] & mask)
                << "Mismatch at index " << i
                << " with usedBits=" << usedBits;
        }

        EXPECT_EQ(decEnd, decoded.data() + values.size());
    }
};

// Basic functionality tests
TEST_F(BitPackEncoderTest, EmptyInput)
{
    TestRoundTrip({}, 8);
}

TEST_F(BitPackEncoderTest, SingleValue)
{
    TestRoundTrip({42}, 8);
    TestRoundTrip({255}, 8);
    TestRoundTrip({1}, 1);
}

TEST_F(BitPackEncoderTest, TwoValues)
{
    TestRoundTrip({10, 20}, 8);
    TestRoundTrip({255, 128}, 8);
}

// Different bit widths
TEST_F(BitPackEncoderTest, OneBit)
{
    TestRoundTrip({0, 1, 1, 0, 1, 0, 0, 1}, 1);
}

TEST_F(BitPackEncoderTest, FourBits)
{
    TestRoundTrip({15, 8, 3, 12, 6, 9, 1, 14}, 4);
}

TEST_F(BitPackEncoderTest, EightBits)
{
    TestRoundTrip({0, 1, 127, 128, 255, 100, 200, 50}, 8);
}

TEST_F(BitPackEncoderTest, SixteenBits)
{
    TestRoundTrip({0, 100, 1000, 10000, 65535, 32768, 1234, 5678}, 16);
}

TEST_F(BitPackEncoderTest, ThirtyTwoBits)
{
    TestRoundTrip({0, 1, UINT32_MAX, 0x12345678, 0xABCDEF00}, 32);
}

// Boundary cases
TEST_F(BitPackEncoderTest, SevenBits)
{
    TestRoundTrip({0, 127, 64, 32, 16, 8, 4, 2, 1}, 7);
}

TEST_F(BitPackEncoderTest, NineBits)
{
    TestRoundTrip({0, 511, 256, 128, 64, 32, 16}, 9);
}

TEST_F(BitPackEncoderTest, FifteenBits)
{
    TestRoundTrip({0, 32767, 16384, 8192, 4096}, 15);
}

TEST_F(BitPackEncoderTest, TwentyFourBits)
{
    TestRoundTrip({0, 16777215, 8388608, 123456, 987654}, 24);
}

// Edge cases - values that span word boundaries
TEST_F(BitPackEncoderTest, SpanWordBoundary_8Bits)
{
    // 8 values of 8 bits = exactly 64 bits (one u64)
    TestRoundTrip({1, 2, 3, 4, 5, 6, 7, 8}, 8);
}

TEST_F(BitPackEncoderTest, SpanWordBoundary_13Bits)
{
    // 13 bits * 5 = 65 bits (spans two u64s)
    TestRoundTrip({100, 200, 300, 400, 500}, 13);
}

TEST_F(BitPackEncoderTest, SpanWordBoundary_17Bits)
{
    // Creates various spanning scenarios
    TestRoundTrip({10000, 20000, 30000, 40000, 50000, 60000, 70000}, 17);
}

// Large datasets
TEST_F(BitPackEncoderTest, LargeDataset_1Bit)
{
    std::vector<u32> values(1000);
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = i % 2;
    }
    TestRoundTrip(values, 1);
}

TEST_F(BitPackEncoderTest, LargeDataset_5Bits)
{
    std::vector<u32> values(500);
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = i % 32;
    }
    TestRoundTrip(values, 5);
}

TEST_F(BitPackEncoderTest, LargeDataset_12Bits)
{
    std::vector<u32> values(300);
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = i % 4096;
    }
    TestRoundTrip(values, 12);
}

// Pattern tests
TEST_F(BitPackEncoderTest, AllZeros)
{
    TestRoundTrip(std::vector<u32>(100, 0), 8);
}

TEST_F(BitPackEncoderTest, AllOnes)
{
    TestRoundTrip(std::vector<u32>(100, 255), 8);
}

TEST_F(BitPackEncoderTest, AlternatingPattern)
{
    std::vector<u32> values(100);
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = (i % 2) ? 255 : 0;
    }
    TestRoundTrip(values, 8);
}

TEST_F(BitPackEncoderTest, IncreasingSequence)
{
    std::vector<u32> values(256);
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = i;
    }
    TestRoundTrip(values, 8);
}

// Special alignment tests
TEST_F(BitPackEncoderTest, ExactlyOneWord)
{
    // 64 bits / 8 bits = 8 values
    TestRoundTrip({11, 22, 33, 44, 55, 66, 77, 88}, 8);
}

TEST_F(BitPackEncoderTest, ExactlyTwoWords)
{
    // 128 bits / 8 bits = 16 values
    std::vector<u32> values(16);
    for (size_t i = 0; i < 16; i++)
    {
        values[i] = i * 10;
    }
    TestRoundTrip(values, 8);
}

// Maximum value tests
TEST_F(BitPackEncoderTest, MaxValues_4Bits)
{
    TestRoundTrip({15, 15, 15, 15, 15}, 4);
}

TEST_F(BitPackEncoderTest, MaxValues_12Bits)
{
    TestRoundTrip({4095, 4095, 4095, 4095}, 12);
}

TEST_F(BitPackEncoderTest, MaxValues_20Bits)
{
    TestRoundTrip({1048575, 1048575, 1048575}, 20);
}

// Comprehensive bit width sweep
TEST_F(BitPackEncoderTest, AllBitWidths)
{
    std::vector<u32> values = {100, 200, 300, 400, 500, 600, 700, 800};

    for (u32 bits = 10; bits <= 32; bits++)
    {
        TestRoundTrip(values, bits);
    }
}

// Test multiple encoder instances (stateless verification)
TEST_F(BitPackEncoderTest, MultipleEncoderInstances)
{
    BitPackEncoder<u32> encoder1, encoder2;
    std::vector<u32> input_data = {10, 20, 30, 40, 50};
    std::vector<u64> encoded1(10, 0), encoded2(10, 0);
    std::vector<u32> decoded1(5, 0), decoded2(5, 0);

    // Encode with both instances
    encoder1.ScalarEncode(encoded1.data(), input_data.data(), 5, 8);
    encoder2.ScalarEncode(encoded2.data(), input_data.data(), 5, 8);

    // Verify encoded data is identical
    EXPECT_EQ(encoded1, encoded2);

    // Decode with both instances
    encoder1.ScalarDecode(decoded1.data(), encoded1.data(), 5, 8);
    encoder2.ScalarDecode(decoded2.data(), encoded2.data(), 5, 8);

    // Verify decoded data matches original
    EXPECT_EQ(decoded1, input_data);
    EXPECT_EQ(decoded2, input_data);
}

// Test Encode/Decode with different encoder instances
TEST_F(BitPackEncoderTest, CrossEncoderCompatibility)
{
    BitPackEncoder<u32> encoder_a, encoder_b;
    std::vector<u32> input_data = {123, 456, 789, 1011, 1213};
    std::vector<u64> encoded(10, 0);
    std::vector<u32> decoded(5, 0);

    // Encode with one instance, Decode with another
    encoder_a.ScalarEncode(encoded.data(), input_data.data(), 5, 16);
    encoder_b.ScalarDecode(decoded.data(), encoded.data(), 5, 16);

    // Verify round-trip
    for (size_t i = 0; i < input_data.size(); i++)
    {
        EXPECT_EQ(decoded[i], input_data[i] & 0xFFFF);
    }
}