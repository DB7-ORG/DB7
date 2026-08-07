#pragma once

#include "access/access_common.hpp"
#include "shared/macro_helper.hpp"
#include "access/data_chunk.hpp"
#include "storage/varlen_entry.hpp"
#include "access/schema.hpp"
#include "shared/byte_utils.hpp"
#include "access/index/header.hpp"

#include <span>
#include <cstring>
#include <utf8proc.h>
#include <cmath>

namespace db7::access
{
    struct KeySpecs
    {
        bool is_nullable;
        bool is_case_sensitive;
    };

    /**
     * understand these
     * UTF8PROC_STABLE
     * UTF8PROC_COMPAT
     * UTF8PROC_IGNORE
     * UTF8PROC_REJECTNA
     * UTF8PROC_NLF2LS
     */

    /** Return a result with decomposed characters. */
    // UTF8PROC_COMPOSE   = (1<<3),
    /** Return a result with decomposed characters. */
    // UTF8PROC_DECOMPOSE = (1<<4),

    struct KeyNormEncoder
    {
    private:
        static u32 EncodeStringNormalized(byte *buf, std::span<const byte> data, bool is_case_sensitive)
        {
            auto opts = static_cast<utf8proc_option_t>(
                UTF8PROC_DECOMPOSE | UTF8PROC_STABLE |
                (is_case_sensitive ? 0 : UTF8PROC_CASEFOLD));

            auto *cp = reinterpret_cast<utf8proc_int32_t *>(buf);

            utf8proc_ssize_t n = utf8proc_decompose(
                reinterpret_cast<const utf8proc_uint8_t *>(data.data()),
                static_cast<utf8proc_ssize_t>(data.size()),
                cp, std::numeric_limits<utf8proc_ssize_t>::max(), opts);
            if (n < 0)
                throw std::runtime_error(std::string("decompose: ") + utf8proc_errmsg(n));

            n = utf8proc_reencode(cp, n, opts);
            if (n < 0)
                throw std::runtime_error(std::string("reencode: ") + utf8proc_errmsg(n));

            return static_cast<u32>(n + 1);
        }

    public:
        template <typename T>
        static u32 EncodeUnsigned(byte *buf, T data)
        {
            T swapped = shared::ByteUtil::ByteSwapIfLittleEndian(data);
            std::memcpy(buf, &swapped, sizeof(T));
            return sizeof(T);
        }

        template <typename T>
        static u32 Encode(byte *buf, T data, bool is_data_null, KeySpecs specs)
        {
            u32 size = 0;

            if (specs.is_nullable)
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
                UTyp swapped = shared::ByteUtil::ByteSwapIfLittleEndian(u);
                std::memcpy(buf, &swapped, sizeof(UTyp));
                size += sizeof(T);
            }
            else if constexpr (std::is_unsigned_v<T>)
            { // u32, u64...
                size += EncodeUnsigned(buf, data);
            }
            else if constexpr (std::is_same_v<T, double>)
            { // double
                double d = data;
                if (d == 0.0)
                    d = 0.0;
                else if (std::isnan(d))
                    d = std::numeric_limits<double>::quiet_NaN();
                uint64_t u;
                std::memcpy(&u, &d, sizeof(u));
                uint64_t mask = (u >> 63) ? ~u64(0) : (u64(1) << 63);
                u ^= mask;
                u = shared::ByteUtil::ByteSwapIfLittleEndian(u);
                std::memcpy(buf, &u, sizeof(u));
                size += sizeof(T);
            }
            else if constexpr (std::is_same_v<T, std::span<const byte>> || std::is_same_v<T, std::span<byte>>)
            { // strings, bytes ...
                // TODO can be optimized heavily for ascii
                size += EncodeStringNormalized(buf, data, specs.is_case_sensitive);
            }
            else
            {
                DB7_UNREACHABLE();
            }

            return size;
        }

        static u16 MaxEncodedSize(const TypeSize &t, KeySpecs specs)
        {
            u16 n = specs.is_nullable ? 1 : 0;

            switch (t.type)
            {
            case type_id::BOOLEAN:
            case type_id::TINYINT:
            case type_id::UTINYINT:
                return n + 1;
            case type_id::SMALLINT:
            case type_id::USMALLINT:
                return n + 2;
            case type_id::INTEGER:
            case type_id::UINTEGER:
                return n + 4;
            case type_id::BIGINT:
            case type_id::UBIGINT:
            case type_id::DOUBLE:
                return n + 8;

            case type_id::VARCHAR:
            case type_id::VARBINARY:
                // NFD ≤ 3x codepoints, full casefold ≤ 3x; 8x input bytes is a safe
                // ceiling with margin. +1 for the 0x00 terminator.
                return n + 4 * (12 + 1);

            default:
                DB7_UNREACHABLE();
            }
        }

        static u16 MaxKeyLen(std::span<const TypeSize> types)
        {
            u16 n = 0;
            for (const auto &t : types)
                n += MaxEncodedSize(t, {false, false});
            return n + sizeof(u64); // the tid suffix
        }

        static u32 SwitchType(byte *buf, void *ptr, bool is_data_null, type_id type, KeySpecs specs)
        {
            switch (type)
            {
            case type_id::BOOLEAN:
            case type_id::UTINYINT:
                return Encode(buf, *(u8 *)ptr, is_data_null, specs);

            case type_id::TINYINT:
                return Encode(buf, *(i8 *)ptr, is_data_null, specs);

            case type_id::USMALLINT:
                return Encode(buf, *(u16 *)ptr, is_data_null, specs);

            case type_id::SMALLINT:
                return Encode(buf, *(i16 *)ptr, is_data_null, specs);

            case type_id::UINTEGER:
                return Encode(buf, *(u32 *)ptr, is_data_null, specs);

            case type_id::INTEGER:
                return Encode(buf, *(i32 *)ptr, is_data_null, specs);

            case type_id::UBIGINT:
                return Encode(buf, *(u64 *)ptr, is_data_null, specs);

            case type_id::BIGINT:
                return Encode(buf, *(i64 *)ptr, is_data_null, specs);

            case type_id::DOUBLE:
                return Encode(buf, *(double *)ptr, is_data_null, specs);

            case type_id::VARCHAR:
            case type_id::VARBINARY:
            {
                auto *entry = (storage::VarlenEntry *)ptr;
                std::span<const byte> data = {(const byte *)entry->GetInline(), entry->GetSize()};
                return Encode(buf, data, is_data_null, specs);
            }
            default:
                DB7_UNREACHABLE();
            }
        }

        static Key BuildKey(byte *out, DataChunk *chunk, u64 key_value, std::vector<TypeSize> &types)
        {
            byte *cur = out;

            for (size_t i = 0; i < types.size(); i++)
            {
                cur += SwitchType(cur, chunk->Get(types[i].col_id), false, types[i].type, {false, false});
            }

            cur += EncodeUnsigned(cur, key_value);

            // NOTE: i removed original value since it only makes sense for index only scans which are rare
            // might add it later if needed fr now keep it simple
            // for (size_t i = 0; i < types.size(); i++)
            // {
            //     memcpy(cur, chunk->Get(types[i].col_id), types[i].size);
            //     cur += types[i].size;
            // }

            u16 len = u16(cur - out);

            return Key{len, out};
        }
    };
}
