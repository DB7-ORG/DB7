
#include "avx2bitpacking_definitions.hpp"

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

    AvxPack<u32>(data, (__m256i *)out, tuple_num, 2);
}
