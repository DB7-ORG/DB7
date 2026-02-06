#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include "../src/storage/compressions/compression.h"

class DictionaryEncoderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Helper to create test strings
    }

    void TearDown() override
    {
        // Clean up any allocated memory
        for (auto ptr : allocatedStrings)
        {
            delete[] ptr;
        }
        allocatedStrings.clear();
    }

    // Helper to create a string on heap
    u8 *makeString(const char *str)
    {
        u32 len = strlen(str);
        u8 *result = new u8[len + 1];
        memcpy(result, str, len + 1);
        allocatedStrings.push_back(result);
        return result;
    }

    std::vector<u8 *> allocatedStrings;
};
// Test: Round-trip encode/decode single string
TEST_F(DictionaryEncoderTest, RoundTripSingleString)
{
    u32 encoded[100];
    u8 *str1 = makeString("test");
    u8 *inStrings[] = {str1};
    u32 inLengths[] = {4};

    u32 totalStrLen = 0;
    for (int i = 0; i < 1; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, 1, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[10];
    u32 outLengths[10];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, 1);

    EXPECT_NE(decodeEnd, nullptr);
    EXPECT_EQ(outLengths[0], 4u);
    EXPECT_EQ(memcmp(outStrings[0], "test", 4), 0);

    // NO CLEANUP NEEDED - outStrings[0] points into 'encoded' buffer
}

// Test: Round-trip multiple unique strings
TEST_F(DictionaryEncoderTest, RoundTripMultipleUniqueStrings)
{
    u32 encoded[1000];
    u8 *str1 = makeString("one");
    u8 *str2 = makeString("two");
    u8 *str3 = makeString("three");
    u8 *inStrings[] = {str1, str2, str3};
    u32 inLengths[] = {3, 3, 5};

    u32 totalStrLen = 0;
    for (int i = 0; i < 3; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, 3, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[10];
    u32 outLengths[10];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, 3);

    EXPECT_NE(decodeEnd, nullptr);
    EXPECT_EQ(outLengths[0], 3u);
    EXPECT_EQ(outLengths[1], 3u);
    EXPECT_EQ(outLengths[2], 5u);
    EXPECT_EQ(memcmp(outStrings[0], "one", 3), 0);
    EXPECT_EQ(memcmp(outStrings[1], "two", 3), 0);
    EXPECT_EQ(memcmp(outStrings[2], "three", 5), 0);

    // NO CLEANUP
}

// Test: Round-trip with duplicates
TEST_F(DictionaryEncoderTest, RoundTripWithDuplicates)
{
    u32 encoded[1000];
    u8 *str1 = makeString("dup");
    u8 *str2 = makeString("unique");
    u8 *str3 = makeString("dup");
    u8 *str4 = makeString("dup");
    u8 *inStrings[] = {str1, str2, str3, str4};
    u32 inLengths[] = {3, 6, 3, 3};

    u32 totalStrLen = 0;
    for (int i = 0; i < 4; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, 4, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[10];
    u32 outLengths[10];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, 4);

    EXPECT_NE(decodeEnd, nullptr);
    EXPECT_EQ(outLengths[0], 3u);
    EXPECT_EQ(outLengths[1], 6u);
    EXPECT_EQ(outLengths[2], 3u);
    EXPECT_EQ(outLengths[3], 3u);
    EXPECT_EQ(memcmp(outStrings[0], "dup", 3), 0);
    EXPECT_EQ(memcmp(outStrings[1], "unique", 6), 0);
    EXPECT_EQ(memcmp(outStrings[2], "dup", 3), 0);
    EXPECT_EQ(memcmp(outStrings[3], "dup", 3), 0);

    // NO CLEANUP
}

// Test: Round-trip with empty strings
TEST_F(DictionaryEncoderTest, RoundTripEmptyStrings)
{
    u32 encoded[100];
    u8 *str1 = makeString("");
    u8 *str2 = makeString("data");
    u8 *str3 = makeString("");
    u8 *inStrings[] = {str1, str2, str3};
    u32 inLengths[] = {0, 4, 0};

    u32 totalStrLen = 0;
    for (int i = 0; i < 3; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, 3, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[10];
    u32 outLengths[10];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, 3);

    EXPECT_NE(decodeEnd, nullptr);
    EXPECT_EQ(outLengths[0], 0u);
    EXPECT_EQ(outLengths[1], 4u);
    EXPECT_EQ(outLengths[2], 0u);
    EXPECT_EQ(memcmp(outStrings[1], "data", 4), 0);

    // NO CLEANUP
}

// Test: Large dataset round-trip
TEST_F(DictionaryEncoderTest, RoundTripLargeDataset)
{
    const int COUNT = 100;
    u32 encoded[10000];
    u8 *inStrings[COUNT];
    u32 inLengths[COUNT];

    for (int i = 0; i < COUNT; i++)
    {
        std::string str = "string_" + std::to_string(i % 10);
        inStrings[i] = makeString(str.c_str());
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[COUNT];
    u32 outLengths[COUNT];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, COUNT);

    EXPECT_NE(decodeEnd, nullptr);

    for (int i = 0; i < COUNT; i++)
    {
        EXPECT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }

    // NO CLEANUP
}

// Test: Binary data
TEST_F(DictionaryEncoderTest, RoundTripBinaryData)
{
    u32 encoded[1000];
    u8 binary1[] = {0x00, 0xFF, 0xAB, 0xCD};
    u8 binary2[] = {0x12, 0x34, 0x56, 0x78};
    u8 *inStrings[] = {binary1, binary2};
    u32 inLengths[] = {4, 4};

    u32 totalStrLen = 0;
    for (int i = 0; i < 2; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryEncoder::encode(encoded, inStrings, inLengths, 2, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    u8 *outStrings[10];
    u32 outLengths[10];
    u32 *decodeEnd = DictionaryEncoder::decode(outStrings, outLengths, encoded, 2);

    EXPECT_NE(decodeEnd, nullptr);
    EXPECT_EQ(outLengths[0], 4u);
    EXPECT_EQ(outLengths[1], 4u);
    EXPECT_EQ(memcmp(outStrings[0], binary1, 4), 0);
    EXPECT_EQ(memcmp(outStrings[1], binary2, 4), 0);

    // NO CLEANUP
}
