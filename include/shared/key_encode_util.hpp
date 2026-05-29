#pragma once

#include "common.hpp"
#include "shared/macro_helper.hpp"

#include <span>
#include <cstring>

namespace db7::shared
{
    struct KeyNormEncoder
    {
    private:
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

    public:
        template <typename T>
        static u32 Encode(byte *buf, T data, bool is_data_null, bool is_nullable, bool is_case_sensitive)
        {
            u32 size = 0;

            if (is_nullable)
            {
                buf[0] = (is_data_null) ? 0x00 : 0x01;
                size++;
                if (is_data_null)
                    return size;
                buf++;
            }

            if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            { // i32, i64...
                using UTyp = std::make_unsigned_t<T>;
                UTyp u;
                std::memcpy(&u, &data, sizeof(T));
                u ^= (UTyp(1) << (sizeof(UTyp) * 8 - 1));
                UTyp swapped = ByteSwapIfLittleEndian(u);
                std::memcpy(buf, &swapped, sizeof(UTyp));
                size += sizeof(T);
            }
            else if constexpr (std::is_unsigned_v<T>)
            { // u32, u64...
                T swapped = ByteSwapIfLittleEndian(data);
                std::memcpy(buf, &swapped, sizeof(T));
                size += sizeof(T);
            }
            else if constexpr (std::is_same_v<T, double>)
            { // double
                uint64_t u;
                std::memcpy(&u, &data, sizeof(u));
                uint64_t mask = (u >> 63) ? ~u64(0) : (u64(1) << 63);
                u ^= mask;
                u = ByteSwapIfLittleEndian(u);
                std::memcpy(buf, &u, sizeof(u));
                size += sizeof(T);
            }
            else if constexpr (std::is_same_v<T, std::span<const byte>>)
            { // strings, bytes ...
                if (is_case_sensitive)
                {
                    std::memcpy(buf, data.data(), data.size());
                }
                else
                {
                    for (size_t i = 0; i < data.size(); i++)
                        buf[i] = std::tolower(data[i]);
                }
                buf[data.size()] = 0x00;
                size += data.size() + 1;
            }
            else
            {
                DB7_UNREACHABLE();
            }

            return size;
        }
    };
}