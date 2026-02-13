#include <gtest/gtest.h>
#include <cstdlib>
#include "../src/storage/compressions/dictionary.hpp"

template <typename ValueType>
class DictionaryTest : public ::testing::Test
{
protected:
    DictionaryValueEncodedRes<ValueType> encoded{};
    ValueType *decoded = nullptr;

    void roundtrip(ValueType *input, u32 count, u32 expectedDistinct)
    {
        encoded.codes = (ValueType *)malloc(count * sizeof(ValueType));
        encoded.values = (ValueType *)malloc(count * sizeof(ValueType));
        encoded.valCount = 0;

        DictionaryValueEncoder<ValueType>::Encode(&encoded, input, count);
        ASSERT_EQ(encoded.valCount, expectedDistinct);

        decoded = (ValueType *)malloc(count * sizeof(ValueType));
        DictionaryValueEncoder<ValueType>::Decode(decoded, &encoded, count);

        for (u32 i = 0; i < count; i++)
        {
            ASSERT_EQ(decoded[i], input[i]) << "Mismatch at index " << i;
        }
    }

    void TearDown() override
    {
        free(encoded.codes);
        free(encoded.values);
        free(decoded);
    }
};

using DictU8 = DictionaryTest<u8>;
using DictU16 = DictionaryTest<u16>;
using DictU32 = DictionaryTest<u32>;

TEST_F(DictU32, AllSame)
{
    u32 data[] = {7, 7, 7, 7, 7};
    roundtrip(data, 5, 1);
}

TEST_F(DictU16, AllUnique)
{
    u16 data[] = {10, 20, 30, 40, 50};
    roundtrip(data, 5, 5);
}

TEST_F(DictU32, SingleElement)
{
    u32 data[] = {42};
    roundtrip(data, 1, 1);
}

TEST_F(DictU8, Alternating)
{
    u8 data[] = {1, 2, 1, 2, 1, 2, 1, 2};
    roundtrip(data, 8, 2);
}

TEST_F(DictU32, RunsThenChange)
{
    u32 data[] = {5, 5, 5, 10, 10, 10, 15, 15, 15};
    roundtrip(data, 9, 3);
}

TEST_F(DictU16, LargeValues)
{
    u16 data[] = {65535, 0, 65535, 0, 32000};
    roundtrip(data, 5, 3);
}

TEST_F(DictU8, MaxDistinct)
{
    u8 data[256];
    for (int i = 0; i < 256; i++)
        data[i] = (u8)i;
    roundtrip(data, 256, 256);
}

TEST_F(DictU32, RepeatedPattern)
{
    u32 data[] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
    roundtrip(data, 9, 3);
}