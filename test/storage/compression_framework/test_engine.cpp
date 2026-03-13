#include <gtest/gtest.h>
#include "../src/storage/compression_framework/engine.hpp"
#include "storage/disk_manager.hpp"

constexpr long tuple_num = AlignUp(120'000, 256);

template <typename type>
void BASE_TEST(type *data, SrcType srcType)
{
    SlabArena arena(100'000);
    ValidityMask validity(tuple_num);

    NumberStats s;
    s.GenerateStats(data, &validity, tuple_num);

    auto estimator = EstimateCostVisitor(&s);
    auto node = IntegerNode(arena);
    u32 estimatedSize = node.Accept(estimator);
    EXPECT_GT(estimatedSize, 0u);
    EXPECT_LT(estimatedSize, tuple_num * sizeof(type));

    auto out = (u8 *)malloc(tuple_num * 10 * sizeof(type));
    auto compressor = CompressVisitor(&s, srcType, data, &validity, tuple_num, out, &arena);
    node.Accept(compressor);

    auto decoder = DecompressVisitor(srcType, &validity, tuple_num, out, compressor.init_header, compressor.init_offsets, &arena);
    node.Accept(decoder);

    type *decoded = (type *)node.buf;
    for (int i = 0; i < tuple_num; i++)
        EXPECT_EQ(decoded[i], data[i]) << "mismatch at index " << i;

    free(out);
}

TEST(CompressionTree, U32RunLengthData)
{
    using type = u32;

    type *data = (type *)malloc(tuple_num * sizeof(type));

    for (int i = 0; i < tuple_num; i++)
        data[i] = (i / 222) + 1;

    BASE_TEST(data, SrcType::U32);

    free(data);
}

TEST(CompressionTree, U64DictData)
{
    using type = u64;

    type *data = (type *)malloc(tuple_num * sizeof(type));

    std::vector<u64> vec(10);
    type idx = 5'000'000;
    for (auto &item : vec)
    {
        item = ++idx;
    }

    srand(42);

    for (int i = 0; i < tuple_num; i++)
        data[i] = vec[rand() % 10];

    BASE_TEST(data, SrcType::U64);

    free(data);
}