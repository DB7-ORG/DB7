
#include "avx2bitpacking_definitions.hpp"
#include <iostream>
constexpr inline size_t AlignUp(const size_t value, const size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

int main()
{
    constexpr long tuple_num = AlignUp(2048, 256);
    auto data = (u32 *)malloc(tuple_num * sizeof(u32));
    auto out = (u32 *)malloc(tuple_num * sizeof(u32));

    for (int i = 0; i < tuple_num; i++)
    {
        data[i] = i % (3);
    }

    constexpr auto usedBits = 5;

    for (u32 i = 0; i < tuple_num / 256; ++i)
    {
        AvxUnPackBlock<u32, usedBits>((__m256i *)data + i * usedBits, out + i * 256);
    }

    // for (u32 i = 0; i < tuple_num / 256; ++i)
    // {
    //     // decoded[i] = BitPackEncoder<type>::DecodeSingle(out, i, usedBits);
    //     constexpr u32 ELEMENTS_PER_VECTOR = 8;
    //     AvxUnPackBlockTemp<u32, usedBits>(data + i * usedBits * ELEMENTS_PER_VECTOR, out + i * 256);
    // }

    for (u32 i = 0; i < tuple_num; i++)
    {
        std::cout << out[i] << std::endl;
    }

    // constexpr auto usedBits1 = 11;

    // for (u32 i = 0; i < tuple_num / 256; ++i)
    // {
    //     // decoded[i] = BitPackEncoder<type>::DecodeSingle(out, i, usedBits);
    //     constexpr u32 ELEMENTS_PER_VECTOR = 8;
    //     AvxUnPackBlockTemp<u32, usedBits1>(data + i * usedBits1 * ELEMENTS_PER_VECTOR, out + i * 256);
    // }

    // for (u32 i = 0; i < tuple_num; i++)
    // {
    //     std::cout << out[i] << std::endl;
    // }

    // constexpr auto usedBits2 = 23;

    // for (u32 i = 0; i < tuple_num / 256; ++i)
    // {
    //     // decoded[i] = BitPackEncoder<type>::DecodeSingle(out, i, usedBits);
    //     constexpr u32 ELEMENTS_PER_VECTOR = 8;
    //     AvxUnPackBlockTemp<u32, usedBits2>(data + i * usedBits2 * ELEMENTS_PER_VECTOR, out + i * 256);
    // }

    // for (u32 i = 0; i < tuple_num; i++)
    // {
    //     std::cout << out[i] << std::endl;
    // }
}
