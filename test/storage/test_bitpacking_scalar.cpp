#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include "../src/storage/compressions/compression.hpp"
#include "bit_utils.hpp"

// Type aliases to match your code
using u64 = uint64_t;
using u32 = uint32_t;

// Test fixture
class BitPackEncoderTest : public ::testing::Test
{
protected:
    BitPackEncoder<u32> encoder;
    std::vector<u32> input;
    std::vector<u32> encoded;
    std::vector<u32> decoded;

    void SetUp() override
    {
        input.clear();
        encoded.clear();
        decoded.clear();
    }

    void TestRoundTrip(const std::vector<u32> &values, u32 usedBits, const std::string &label = "")
    {
        input = values;

        // Calculate maximum encoded size
        encoded.resize(values.size(), 0);
        decoded.resize(values.size(), 0);

        // Encode
        u32 *encEnd = encoder.ScalarEncode(encoded.data(), input.data(),
                                           input.size(), usedBits);
        (void)encEnd;
        // Decode
        u32 *decEnd = encoder.ScalarDecode(decoded.data(), encoded.data(),
                                           input.size(), usedBits);

        // Verify
        for (size_t i = 0; i < values.size(); i++)
        {
            EXPECT_EQ(decoded[i], values[i])
                << "TestRoundTrip[" << label << "] mismatch at index " << i
                << " with usedBits=" << usedBits
                << " expected=" << values[i]
                << " got=" << decoded[i];
        }

        EXPECT_EQ(decEnd, decoded.data() + values.size())
            << label;
    }
};

TEST_F(BitPackEncoderTest, EmptyInput)
{
    TestRoundTrip({}, 0, "EmptyInput");
}

TEST_F(BitPackEncoderTest, SingleElement)
{
    TestRoundTrip({1}, 1, "SingleElement");
}

TEST_F(BitPackEncoderTest, SimpleCase)
{
    TestRoundTrip({0, 1, 2, 3, 4, 5, 6, 7}, 3, "SimpleCase");
}

TEST_F(BitPackEncoderTest, SplittingCase)
{
    u32 nitems = 500;
    u32 mod = 64;
    std::vector<u32> data(nitems);
    for (u32 i = 0; i < nitems; i++)
    {
        data[i] = i % (mod);
    }

    TestRoundTrip(data, CountBitsUsed(mod - 1), "SplittingCase");
}

TEST_F(BitPackEncoderTest, GeneralCase)
{
    u32 nitems = 120'000;
    u32 mod = 222;
    std::vector<u32> data(nitems);
    for (u32 i = 0; i < nitems; i++)
    {
        data[i] = i % mod;
    }

    TestRoundTrip(data, CountBitsUsed(mod - 1), "GeneralCase");
}