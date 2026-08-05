#pragma once

namespace db7::shared
{
    class ByteUtil
    {
    public:
        template <typename T>
        static T ByteSwapIfLittleEndian(T val)
        {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            if constexpr (sizeof(T) == 1)
                return val;
            if constexpr (sizeof(T) == 2)
                return __builtin_bswap16(val);
            if constexpr (sizeof(T) == 4)
                return __builtin_bswap32(val);
            if constexpr (sizeof(T) == 8)
                return __builtin_bswap64(val);
#endif
            return val;
        }
    };
}