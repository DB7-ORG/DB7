#include <gtest/gtest.h>
#include "../src/storage/compressions/compression.hpp"

template <typename T>
struct BitpackCombinedTest : public ::testing::Test
{
    static constexpr u32 EXTRA = 32; // safety margin for AVX overwrite

    void RoundTrip(const std::vector<T> &input, u32 usedBits)
    {
        u32 nitems = input.size();
        std::vector<u8> encoded(nitems * sizeof(T) * 2, 0);
        std::vector<T> decoded(nitems + EXTRA, 0);

        BitPackCombinedEncoder<T>::Encode(encoded.data(), input.data(), nitems, usedBits);
        BitPackCombinedEncoder<T>::Decode(decoded.data(), encoded.data(), nitems, usedBits);

        for (u32 i = 0; i < nitems; i++)
            ASSERT_EQ(decoded[i], input[i]) << "mismatch at index " << i;
    }
};

using BitpackTypes = ::testing::Types<u16, u32, u64>;
TYPED_TEST_SUITE(BitpackCombinedTest, BitpackTypes);

TYPED_TEST(BitpackCombinedTest, SingleElement)
{
    this->RoundTrip({42}, CountBitsUsed((TypeParam)42));
}

TYPED_TEST(BitpackCombinedTest, AllZeros)
{
    this->RoundTrip(std::vector<TypeParam>(256, 0), 1);
}

// ----------------------------------------------------------------
// All same value
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, AllSame)
{
    TypeParam val = 7;
    this->RoundTrip(std::vector<TypeParam>(256, val), CountBitsUsed(val));
}

// ----------------------------------------------------------------
// Exactly one AVX block (256 elements)
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, ExactBlock)
{
    std::vector<TypeParam> data(256);
    TypeParam max = 0;
    for (u32 i = 0; i < 256; i++)
    {
        data[i] = i % 100;
        max = std::max(max, data[i]);
    }
    this->RoundTrip(data, CountBitsUsed(max));
}

// ----------------------------------------------------------------
// Multiple AVX blocks (1024 elements)
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, MultipleBlocks)
{
    std::vector<TypeParam> data(1024);
    TypeParam max = 0;
    for (u32 i = 0; i < 1024; i++)
    {
        data[i] = i % 1000;
        max = std::max(max, data[i]);
    }
    this->RoundTrip(data, CountBitsUsed(max));
}

// ----------------------------------------------------------------
// Leftover elements (not multiple of 256)
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, WithLeftover)
{
    constexpr u32 N = 300; // 256 + 44 leftover
    std::vector<TypeParam> data(N);
    TypeParam max = 0;
    for (u32 i = 0; i < N; i++)
    {
        data[i] = i % 50;
        max = std::max(max, data[i]);
    }
    this->RoundTrip(data, CountBitsUsed(max));
}

// ----------------------------------------------------------------
// Max bit width
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, MaxBits)
{
    constexpr u32 N = 256;
    TypeParam maxVal = std::numeric_limits<TypeParam>::max();
    u32 usedBits = sizeof(TypeParam) * 8;
    this->RoundTrip(std::vector<TypeParam>(N, maxVal), usedBits);
}

// ----------------------------------------------------------------
// Large dataset
// ----------------------------------------------------------------
TYPED_TEST(BitpackCombinedTest, LargeDataset)
{
    constexpr u32 N = 120'000;
    std::vector<TypeParam> data(N);
    TypeParam max = 0;
    for (u32 i = 0; i < N; i++)
    {
        data[i] = i % 10000;
        max = std::max(max, data[i]);
    }
    this->RoundTrip(data, CountBitsUsed(max));
}