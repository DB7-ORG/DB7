#include <gtest/gtest.h>
#include "../src/storage/compressions/compression.hpp"

// // ── Helpers ───────────────────────────────────────────────────────────────────

// template <typename T>
// struct RleBuffers
// {
//     std::vector<T> values;
//     std::vector<u16> counts;
//     RleEncodedRes<T> res;

//     RleBuffers(u32 nitems)
//         : values(nitems), counts(nitems)
//     {
//         res.values = values.data();
//         res.counts = counts.data();
//         res.count = 0;
//     }
// };

// static ValidityMask AllValidMask(u32 nitems)
// {
//     ValidityMask mask(nitems);
//     // assume default is all valid
//     return mask;
// }

// template <typename T>
// std::vector<T> DecodeBuffer(const RleEncodedRes<T> &res, u32 nitems)
// {
//     // +32 bytes extra as required by Decode contract
//     std::vector<T> out(nitems + 32 / sizeof(T), T{});
//     RleEncoder::Decode(out.data(), &res);
//     out.resize(nitems);
//     return out;
// }

// // ── Encode: basic ─────────────────────────────────────────────────────────────

// TEST(RleEncode, SingleElement)
// {
//     std::vector<u32> in = {42};
//     ValidityMask mask = AllValidMask(1);
//     RleBuffers<u32> buf(1);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 1);

//     EXPECT_EQ(buf.res.count, 1u);
//     EXPECT_EQ(buf.values[0], 42u);
//     EXPECT_EQ(buf.counts[0], 1u);
// }

// TEST(RleEncode, AllSameValue)
// {
//     std::vector<u32> in(100, 7u);
//     ValidityMask mask = AllValidMask(100);
//     RleBuffers<u32> buf(100);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 100);

//     EXPECT_EQ(buf.res.count, 1u);
//     EXPECT_EQ(buf.values[0], 7u);
//     EXPECT_EQ(buf.counts[0], 100u);
// }

// TEST(RleEncode, AllDistinct)
// {
//     std::vector<u32> in = {1, 2, 3, 4, 5};
//     ValidityMask mask = AllValidMask(5);
//     RleBuffers<u32> buf(5);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 5);

//     EXPECT_EQ(buf.res.count, 5u);
//     for (u32 i = 0; i < 5; i++)
//     {
//         EXPECT_EQ(buf.values[i], i + 1);
//         EXPECT_EQ(buf.counts[i], 1u);
//     }
// }

// TEST(RleEncode, AlternatingValues)
// {
//     std::vector<u32> in = {1, 2, 1, 2, 1, 2};
//     ValidityMask mask = AllValidMask(6);
//     RleBuffers<u32> buf(6);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 6);

//     EXPECT_EQ(buf.res.count, 6u);
// }

// TEST(RleEncode, MultipleRuns)
// {
//     // 3x1, 2x2, 4x3
//     std::vector<u32> in = {1, 1, 1, 2, 2, 3, 3, 3, 3};
//     ValidityMask mask = AllValidMask(9);
//     RleBuffers<u32> buf(9);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 9);

//     EXPECT_EQ(buf.res.count, 3u);
//     EXPECT_EQ(buf.values[0], 1u);
//     EXPECT_EQ(buf.counts[0], 3u);
//     EXPECT_EQ(buf.values[1], 2u);
//     EXPECT_EQ(buf.counts[1], 2u);
//     EXPECT_EQ(buf.values[2], 3u);
//     EXPECT_EQ(buf.counts[2], 4u);
// }

// TEST(RleEncode, Uint16Type)
// {
//     std::vector<u16> in = {10, 10, 20, 20, 20};
//     ValidityMask mask = AllValidMask(5);
//     RleBuffers<u16> buf(5);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 5);

//     EXPECT_EQ(buf.res.count, 2u);
//     EXPECT_EQ(buf.values[0], 10u);
//     EXPECT_EQ(buf.counts[0], 2u);
//     EXPECT_EQ(buf.values[1], 20u);
//     EXPECT_EQ(buf.counts[1], 3u);
// }

// TEST(RleEncode, Uint64Type)
// {
//     std::vector<u64> in = {0xDEADBEEFull, 0xDEADBEEFull, 0xCAFEull};
//     ValidityMask mask = AllValidMask(3);
//     RleBuffers<u64> buf(3);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, 3);

//     EXPECT_EQ(buf.res.count, 2u);
//     EXPECT_EQ(buf.values[0], 0xDEADBEEFull);
//     EXPECT_EQ(buf.counts[0], 2u);
//     EXPECT_EQ(buf.values[1], 0xCAFEull);
// }

// // ── Encode: UINT16_MAX run cap ────────────────────────────────────────────────

// TEST(RleEncode, RunCapAtUint16Max)
// {
//     u32 nitems = (u32)UINT16_MAX + 4;
//     std::vector<u32> in(nitems, 99u);
//     ValidityMask mask = AllValidMask(nitems);
//     RleBuffers<u32> buf(nitems);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, nitems);

//     // Must have split into at least 2 runs
//     EXPECT_GE(buf.res.count, 2u);
//     // All counts must be <= UINT16_MAX
//     u32 total = 0;
//     for (u32 i = 0; i < buf.res.count; i++)
//     {
//         EXPECT_LE(buf.counts[i], (u16)UINT16_MAX);
//         EXPECT_EQ(buf.values[i], 99u);
//         total += buf.counts[i];
//     }
//     EXPECT_EQ(total, nitems);
// }

// // ── Encode: nullmap ───────────────────────────────────────────────────────────

// TEST(RleEncode, NullRowBreaksRun)
// {
//     std::vector<u32> in = {5, 5, 5, 5, 5};
//     ValidityMask mask(5);
//     mask.SetInvalid(2); // null in the middle

//     RleBuffers<u32> buf(5);
//     RleEncoder::Encode(&buf.res, in.data(), &mask, 5);

//     // null breaks the run of 5s into at least 2 runs
//     EXPECT_GE(buf.res.count, 2u);
// }

// // ── Decode: basic ─────────────────────────────────────────────────────────────

// TEST(RleDecode, SingleRun)
// {
//     std::vector<u32> values = {42};
//     std::vector<u16> counts = {5};
//     RleEncodedRes<u32> res{values.data(), counts.data(), 1};

//     auto out = DecodeBuffer(res, 5);

//     for (u32 i = 0; i < 5; i++)
//         EXPECT_EQ(out[i], 42u);
// }

// TEST(RleDecode, MultipleRuns)
// {
//     std::vector<u32> values = {1, 2, 3};
//     std::vector<u16> counts = {3, 2, 4};
//     RleEncodedRes<u32> res{values.data(), counts.data(), 3};

//     auto out = DecodeBuffer(res, 9);

//     std::vector<u32> expected = {1, 1, 1, 2, 2, 3, 3, 3, 3};
//     EXPECT_EQ(out, expected);
// }

// TEST(RleDecode, Uint16Type)
// {
//     std::vector<u16> values = {10, 20};
//     std::vector<u16> counts = {2, 3};
//     RleEncodedRes<u16> res{values.data(), counts.data(), 2};

//     auto out = DecodeBuffer(res, 5);

//     std::vector<u16> expected = {10, 10, 20, 20, 20};
//     EXPECT_EQ(out, expected);
// }

// // ── Roundtrip ─────────────────────────────────────────────────────────────────

// template <typename T>
// void RoundtripTest(const std::vector<T> &in)
// {
//     u32 nitems = in.size();
//     ValidityMask mask = AllValidMask(nitems);
//     RleBuffers<T> buf(nitems);

//     RleEncoder::Encode(&buf.res, in.data(), &mask, nitems);
//     auto out = DecodeBuffer(buf.res, nitems);

//     EXPECT_EQ(out, in);
// }

// TEST(RleRoundtrip, AllSame) { RoundtripTest<u32>({5, 5, 5, 5, 5}); }
// TEST(RleRoundtrip, AllDistinct) { RoundtripTest<u32>({1, 2, 3, 4, 5}); }
// TEST(RleRoundtrip, MultipleRuns) { RoundtripTest<u32>({1, 1, 2, 2, 2, 3}); }
// TEST(RleRoundtrip, SingleElement) { RoundtripTest<u32>({99}); }
// TEST(RleRoundtrip, Uint16) { RoundtripTest<u16>({10, 10, 20, 30, 30, 30}); }
// TEST(RleRoundtrip, Uint64) { RoundtripTest<u64>({0xABCDull, 0xABCDull, 0x1234ull}); }

// TEST(RleRoundtrip, LargeInput)
// {
//     std::vector<u32> in;
//     in.reserve(120'000);
//     for (u32 i = 0; i < 120'000; i++)
//         in.push_back((i / 222) + 1); // same pattern as your test data
//     RoundtripTest<u32>(in);
// }

// TEST(RleRoundtrip, Alternating)
// {
//     std::vector<u32> in;
//     for (u32 i = 0; i < 1000; i++)
//         in.push_back(i % 2);
//     RoundtripTest<u32>(in);
// }