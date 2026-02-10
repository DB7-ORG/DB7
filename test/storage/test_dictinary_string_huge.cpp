#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <random>
#include <sstream>
#include "../src/storage/compressions/compression.h"

class DictionaryStringEncoderHugeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rng.seed(42);
    }

    void TearDown() override
    {
        for (auto ptr : allocatedStrings)
        {
            delete[] ptr;
        }
        allocatedStrings.clear();
    }

    u8 *makeString(const char *str)
    {
        u32 len = strlen(str);
        u8 *result = new u8[len + 1];
        memcpy(result, str, len + 1);
        allocatedStrings.push_back(result);
        return result;
    }

    u8 *makeString(const std::string &str)
    {
        return makeString(str.c_str());
    }

    // Generate random string of given length
    std::string generateRandomString(size_t length)
    {
        const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; i++)
        {
            result += charset[dist(rng)];
        }
        return result;
    }

    std::vector<u8 *> allocatedStrings;
    std::mt19937 rng;
};

// Test: Very large number of identical strings (maximum deduplication)
TEST_F(DictionaryStringEncoderHugeTest, ThousandsOfIdenticalStrings)
{
    const int COUNT = 10000;
    std::vector<u32> encoded(COUNT * 10); // Generous buffer
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    std::string commonStr = "repeated_string_value";
    for (int i = 0; i < COUNT; i++)
    {
        inStrings[i] = makeString(commonStr);
        inLengths[i] = commonStr.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    // Check compression ratio - should be very high
    size_t encodedSize = encodeEnd - encoded.data();
    size_t originalSize = COUNT * commonStr.length();
    double ratio = (double)originalSize / encodedSize;
    EXPECT_GT(ratio, 10.0) << "Compression ratio should be high for identical strings";

    // Decode and verify
    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Very large number of unique strings (no deduplication benefit)
TEST_F(DictionaryStringEncoderHugeTest, ThousandsOfUniqueStrings)
{
    const int COUNT = 5000;
    std::vector<u32> encoded(COUNT * 100); // Large buffer for unique strings
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    for (int i = 0; i < COUNT; i++)
    {
        std::string str = "unique_string_" + std::to_string(i) + "_" + generateRandomString(20);
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// TODO this should be added but later

//  Test: Huge strings (individual strings that are very long)
//  TEST_F(DictionaryStringEncoderHugeTest, VeryLongIndividualStrings)
//  {
//      const int COUNT = 10;
//      const size_t STRING_LENGTH = 100000;             // 100KB strings
//      std::vector<u32> encoded(COUNT * STRING_LENGTH); // Very large buffer
//      std::vector<u8 *> inStrings(COUNT);
//      std::vector<u32> inLengths(COUNT);

//     for (int i = 0; i < COUNT; i++)
//     {
//         std::string str = generateRandomString(STRING_LENGTH);
//         inStrings[i] = makeString(str);
//         inLengths[i] = str.length();
//     }

//     u32 totalStrLen = 0;
//     for (int i = 0; i < COUNT; i++)
//     {
//         totalStrLen += inLengths[i];
//     }

//     u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
//     ASSERT_NE(encodeEnd, nullptr);

//     std::vector<u8 *> outStrings(COUNT);
//     std::vector<u32> outLengths(COUNT);
//     u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

//     EXPECT_NE(decodeEnd, nullptr);
//     for (int i = 0; i < COUNT; i++)
//     {
//         ASSERT_EQ(outLengths[i], inLengths[i]);
//         EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
//     }
// }

// Test: Realistic dataset with Zipf distribution (some strings very common, most rare)
TEST_F(DictionaryStringEncoderHugeTest, ZipfDistributionDataset)
{
    const int COUNT = 10000;
    const int UNIQUE_COUNT = 100;
    std::vector<u32> encoded(COUNT * 20);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    // Create unique strings
    std::vector<std::string> uniqueStrings;
    for (int i = 0; i < UNIQUE_COUNT; i++)
    {
        uniqueStrings.push_back("value_" + std::to_string(i) + "_" + generateRandomString(10));
    }

    // Use Zipf-like distribution: first strings appear much more frequently
    std::vector<int> frequencies(UNIQUE_COUNT);
    for (int i = 0; i < UNIQUE_COUNT; i++)
    {
        frequencies[i] = UNIQUE_COUNT - i;
    }

    int totalFreq = 0;
    for (int f : frequencies)
        totalFreq += f;

    // Distribute strings according to frequencies
    int idx = 0;
    for (int i = 0; i < UNIQUE_COUNT && idx < COUNT; i++)
    {
        int count = (frequencies[i] * COUNT) / totalFreq;
        for (int j = 0; j < count && idx < COUNT; j++)
        {
            inStrings[idx] = makeString(uniqueStrings[i]);
            inLengths[idx] = uniqueStrings[i].length();
            idx++;
        }
    }

    // Fill remaining with random strings
    while (idx < COUNT)
    {
        int strIdx = idx % UNIQUE_COUNT;
        inStrings[idx] = makeString(uniqueStrings[strIdx]);
        inLengths[idx] = uniqueStrings[strIdx].length();
        idx++;
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Mixed string lengths from very short to very long
TEST_F(DictionaryStringEncoderHugeTest, MixedStringLengths)
{
    const int COUNT = 1000;
    std::vector<u32> encoded(COUNT * 1000);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    for (int i = 0; i < COUNT; i++)
    {
        // Lengths: 0, 1, 2, 4, 8, 16, 32, ... up to 10000
        size_t length = (i < 10) ? i : (1 << (i % 14));
        if (length > 10000)
            length = 10000;

        std::string str = generateRandomString(length);
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        if (inLengths[i] > 0)
        {
            EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
        }
    }
}

// Test: Strings with high similarity but not identical
TEST_F(DictionaryStringEncoderHugeTest, HighlySimilarStrings)
{
    const int COUNT = 5000;
    std::vector<u32> encoded(COUNT * 50);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    std::string baseStr = "this_is_a_common_prefix_that_appears_in_many_strings_";
    for (int i = 0; i < COUNT; i++)
    {
        std::string str = baseStr + std::to_string(i);
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Alternating pattern of duplicates
TEST_F(DictionaryStringEncoderHugeTest, AlternatingDuplicatePattern)
{
    const int COUNT = 10000;
    std::vector<u32> encoded(COUNT * 10);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    std::string str1 = "pattern_A";
    std::string str2 = "pattern_B";

    for (int i = 0; i < COUNT; i++)
    {
        if (i % 2 == 0)
        {
            inStrings[i] = makeString(str1);
            inLengths[i] = str1.length();
        }
        else
        {
            inStrings[i] = makeString(str2);
            inLengths[i] = str2.length();
        }
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: All empty strings
TEST_F(DictionaryStringEncoderHugeTest, ThousandsOfEmptyStrings)
{
    const int COUNT = 10000;
    std::vector<u32> encoded(COUNT * 2);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    u8 *emptyStr = makeString("");
    for (int i = 0; i < COUNT; i++)
    {
        inStrings[i] = emptyStr;
        inLengths[i] = 0;
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        EXPECT_EQ(outLengths[i], 0u);
    }
}

// Test: Binary data with null bytes throughout
TEST_F(DictionaryStringEncoderHugeTest, BinaryDataWithNullBytes)
{
    const int COUNT = 1000;
    std::vector<u32> encoded(COUNT * 100);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    for (int i = 0; i < COUNT; i++)
    {
        u32 len = 50;
        u8 *binary = new u8[len];

        // Fill with pattern including null bytes
        for (u32 j = 0; j < len; j++)
        {
            binary[j] = (u8)((i + j) % 256);
        }

        inStrings[i] = binary;
        inLengths[i] = len;
        allocatedStrings.push_back(binary);
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Stress test with categorical data (simulating database column)
TEST_F(DictionaryStringEncoderHugeTest, CategoricalDataSimulation)
{
    const int COUNT = 50000;
    std::vector<u32> encoded(COUNT * 10);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    // Simulate categorical data like status codes, countries, etc.
    std::vector<std::string> categories = {
        "ACTIVE", "INACTIVE", "PENDING", "SUSPENDED", "CLOSED",
        "NEW", "IN_PROGRESS", "COMPLETED", "CANCELLED", "ARCHIVED"};

    std::uniform_int_distribution<> dist(0, categories.size() - 1);

    for (int i = 0; i < COUNT; i++)
    {
        const std::string &cat = categories[dist(rng)];
        inStrings[i] = makeString(cat);
        inLengths[i] = cat.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    // Verify good compression ratio for categorical data
    size_t encodedSize = encodeEnd - encoded.data();
    size_t originalApproxSize = COUNT * 10; // Average category length
    double ratio = (double)originalApproxSize / encodedSize;
    EXPECT_GT(ratio, 5.0) << "Should compress categorical data well";

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Maximum dictionary size stress test
TEST_F(DictionaryStringEncoderHugeTest, MaximumDictionarySizeStress)
{
    const int COUNT = 20000;
    const int UNIQUE_COUNT = 10000; // Large dictionary
    std::vector<u32> encoded(COUNT * 50);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    // Create many unique strings to stress the dictionary capacity
    std::vector<std::string> uniqueStrings;
    for (int i = 0; i < UNIQUE_COUNT; i++)
    {
        uniqueStrings.push_back("dict_entry_" + std::to_string(i) + "_" + generateRandomString(15));
    }

    // Each unique string appears twice
    for (int i = 0; i < COUNT; i++)
    {
        const std::string &str = uniqueStrings[i % UNIQUE_COUNT];
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}

// Test: Unicode and multi-byte characters
TEST_F(DictionaryStringEncoderHugeTest, UnicodeStringsLarge)
{
    const int COUNT = 1000;
    std::vector<u32> encoded(COUNT * 100);
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    std::vector<std::string> unicodeStrings = {
        "Hello 世界",
        "Привет мир",
        "مرحبا العالم",
        "こんにちは世界",
        "🌍🌎🌏",
        "Ñoño español",
        "Café résumé",
        "Ümlaut tëst"};

    for (int i = 0; i < COUNT; i++)
    {
        const std::string &str = unicodeStrings[i % unicodeStrings.size()];
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    u32 *encodeEnd = DictionaryStringEncoder::encode(encoded.data(), inStrings.data(), inLengths.data(), COUNT, totalStrLen);
    ASSERT_NE(encodeEnd, nullptr);

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);
    u32 *decodeEnd = DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), encoded.data(), COUNT);

    EXPECT_NE(decodeEnd, nullptr);
    for (int i = 0; i < COUNT; i++)
    {
        ASSERT_EQ(outLengths[i], inLengths[i]);
        EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0);
    }
}