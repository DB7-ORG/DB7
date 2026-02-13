#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <random>
#include <sstream>
#include "../src/storage/compressions/compression.hpp"

class DictionaryStringEncoderTest : public ::testing::Test
{
protected:
    std::vector<u8 *> allocated;
    std::mt19937 rng{42};

    void TearDown() override
    {
        for (auto ptr : allocated)
            delete[] ptr;
    }

    u8 *make(const std::string &str)
    {
        u8 *p = new u8[str.size() + 1];
        memcpy(p, str.data(), str.size() + 1);
        allocated.push_back(p);
        return p;
    }

    u8 *makeBinary(const std::vector<u8> &bytes)
    {
        u8 *p = new u8[bytes.size()];
        memcpy(p, bytes.data(), bytes.size());
        allocated.push_back(p);
        return p;
    }

    std::string randomString(size_t len)
    {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<> d(0, sizeof(chars) - 2);
        std::string s;
        s.reserve(len);
        for (size_t i = 0; i < len; i++)
            s += chars[d(rng)];
        return s;
    }

    void roundtrip(std::vector<u8 *> &inStrings, std::vector<u32> &inLengths)
    {
        u32 count = inStrings.size();
        u32 totalLen = 0;
        for (u32 l : inLengths)
            totalLen += l;

        auto encoded = DictionaryStringEncodedRes{
            .codes = (u32 *)malloc(count * sizeof(u32)),
            .indexes = (u32 *)malloc((count + 1) * sizeof(u32)),
            .strings = (u8 *)malloc(totalLen ? totalLen : 1),
        };

        DictionaryStringEncoder::encode(&encoded, inStrings.data(), inLengths.data(), count);

        std::vector<u8 *> outStrings(count);
        std::vector<u32> outLengths(count);
        DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), &encoded, count);

        for (u32 i = 0; i < count; i++)
        {
            ASSERT_EQ(outLengths[i], inLengths[i]) << "length mismatch at " << i;
            if (inLengths[i] > 0)
            {
                EXPECT_EQ(memcmp(outStrings[i], inStrings[i], inLengths[i]), 0) << "data mismatch at " << i;
            }
        }

        free(encoded.codes);
        free(encoded.indexes);
        free(encoded.strings);
    }
};

TEST_F(DictionaryStringEncoderTest, SingleString)
{
    std::vector<u8 *> strs = {make("test")};
    std::vector<u32> lens = {4};
    roundtrip(strs, lens);
}

TEST_F(DictionaryStringEncoderTest, EmptyStrings)
{
    std::vector<u8 *> strs = {make(""), make("data"), make("")};
    std::vector<u32> lens = {0, 4, 0};
    roundtrip(strs, lens);
}

TEST_F(DictionaryStringEncoderTest, AllIdentical)
{
    const int COUNT = 10000;
    std::vector<u8 *> strs(COUNT);
    std::vector<u32> lens(COUNT);
    for (int i = 0; i < COUNT; i++)
    {
        strs[i] = make("repeated");
        lens[i] = 8;
    }
    roundtrip(strs, lens);
}

TEST_F(DictionaryStringEncoderTest, AllUnique)
{
    const int COUNT = 5000;
    std::vector<u8 *> strs(COUNT);
    std::vector<u32> lens(COUNT);
    for (int i = 0; i < COUNT; i++)
    {
        std::string s = "u_" + std::to_string(i) + "_" + randomString(20);
        strs[i] = make(s);
        lens[i] = s.size();
    }
    roundtrip(strs, lens);
}

TEST_F(DictionaryStringEncoderTest, MixedDuplication)
{
    // Simulates realistic categorical data: small dictionary, many rows
    const int COUNT = 50000;
    std::vector<std::string> cats = {
        "ACTIVE", "INACTIVE", "PENDING", "SUSPENDED", "CLOSED",
        "NEW", "IN_PROGRESS", "COMPLETED", "CANCELLED", "ARCHIVED"};
    std::uniform_int_distribution<> d(0, cats.size() - 1);

    std::vector<u8 *> strs(COUNT);
    std::vector<u32> lens(COUNT);
    for (int i = 0; i < COUNT; i++)
    {
        const auto &s = cats[d(rng)];
        strs[i] = make(s);
        lens[i] = s.size();
    }
    roundtrip(strs, lens);
}

// TODO this should be added but later

//  Test: Huge strings (individual strings that are very long)
//  TEST_F(DictionaryStringEncoderTest, VeryLongIndividualStrings)
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

TEST_F(DictionaryStringEncoderTest, BinaryDataWithNullBytes)
{
    const int COUNT = 1000;
    std::vector<u8 *> strs(COUNT);
    std::vector<u32> lens(COUNT, 50);
    for (int i = 0; i < COUNT; i++)
    {
        std::vector<u8> bytes(50);
        for (int j = 0; j < 50; j++)
            bytes[j] = (u8)((i + j) % 256);
        strs[i] = makeBinary(bytes);
    }
    roundtrip(strs, lens);
}

TEST_F(DictionaryStringEncoderTest, UnicodeStrings)
{
    std::vector<std::string> src = {
        "Hello 世界", "Привет мир", "مرحبا العالم",
        "こんにちは世界", "🌍🌎🌏", "Café résumé"};

    const int COUNT = 600;
    std::vector<u8 *> strs(COUNT);
    std::vector<u32> lens(COUNT);
    for (int i = 0; i < COUNT; i++)
    {
        const auto &s = src[i % src.size()];
        strs[i] = make(s);
        lens[i] = s.size();
    }
    roundtrip(strs, lens);
}
