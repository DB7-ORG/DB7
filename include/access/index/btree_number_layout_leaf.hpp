#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

namespace db7::access
{

    template <typename T>
    class BtreeNumberLayoutLeaf
    {
        static_assert(std::is_arithmetic_v<T>, "T must be a numeric type");

    private:
        using R = u64;

        static constexpr T UNDEFINED = std::numeric_limits<T>::max();

        u64 key_offset_;
        u64 ref_offset_;
        u64 max_count_;

        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }

        // u32 GetIdx(const T *data, const u32 count, const T value)
        // {
        //     DB7_ASSERT(count != 0, "zero count node");
        //     u32 lo = 0, hi = count;
        //     while (lo < hi)
        //     {
        //         u32 mid = lo + (hi - lo) / 2;
        //         if (data[mid] <= value)
        //             lo = mid + 1;
        //         else
        //             hi = mid;
        //     }
        //     return lo;
        // }

        u32 GetIdx(const T *data, const u32 count, const T value, bool &found)
        {
            DB7_ASSERT(count != 0, "zero count node");
            found = false;
            u32 lo = 0, hi = count;
            while (lo < hi)
            {
                u32 mid = lo + (hi - lo) / 2;
                if (data[mid] == value)
                {
                    found = true;
                    return mid;
                }
                if (data[mid] < value)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        template <typename Typ>
        u32 CopyUpperHalf(Typ *from, Typ *to, u32 count)
        {
            u32 mid = (count + 1) / 2;
            std::memcpy(to, from + mid, (count - mid) * sizeof(Typ));
            return mid;
        }

        T *OffsetKey(byte *data)
        {
            return reinterpret_cast<T *>(data + key_offset_);
        }

        R *OffsetRef(byte *data)
        {
            return reinterpret_cast<R *>(data + ref_offset_);
        }

        T GetKeyAt(byte *data, u32 idx)
        {
            T *arr = OffsetKey(data);
            return arr[idx];
        }

    public:
        BtreeNumberLayoutLeaf(u64 header_size)
        {
            constexpr u64 pad_keys = sizeof(R) - 1;
            key_offset_ = shared::AlignUp(header_size, (u64)sizeof(T));
            max_count_ = (PAGE_SIZE - key_offset_ - pad_keys) / (sizeof(T) + sizeof(R));
            ref_offset_ = shared::AlignUp(key_offset_ + max_count_ * sizeof(T), (u64)sizeof(R));
        }

        // R Get(byte *data, const u32 count, const T value)
        // {
        //     T *arr = OffsetKey(data);
        //     u32 lo = GetIdx(arr, count, value) - 1;
        //     if (lo < count && arr[lo] == value)
        //         return (OffsetRef(data))[lo];
        //     return BtreeNumberLayoutLeaf::UNDEFINED;
        // }
        R Get(byte *data, const u32 count, const T value)
        {
            bool found;
            u32 idx = GetIdx(OffsetKey(data), count, value, found);
            if (found)
                return (OffsetRef(data))[idx];
            return BtreeNumberLayoutLeaf::UNDEFINED;
        }

        void Insert(byte *data, u32 count, T key, R value)
        {
            bool found;
            u32 idx = count == 0 ? 0 : GetIdx(OffsetKey(data), count, key, found);
            ShiftRightInsert(OffsetKey(data), count, idx, key);
            ShiftRightInsert(OffsetRef(data), count, idx, value);
        }

        // void Insert(byte *data, u32 count, T key, R value)
        // {
        //     if (UNLIKELY(count == 0))
        //     {
        //         *OffsetKey(data) = key;
        //         *OffsetRef(data) = value;
        //     }
        //     else
        //     {
        //         u32 idx = GetIdx(OffsetKey(data), count, key);
        //         ShiftRightInsert(OffsetKey(data), count, idx, key);
        //         ShiftRightInsert(OffsetRef(data), count, idx, value);
        //     }
        // }

        bool HasSpace(BtreeHeader<T> *header, T key)
        {
            (void)key;
            return header->count < max_count_;
        }

        void Split(byte *data, byte *right_data, u32 count, T key, page_id value, T &sentinel_out, u32 &new_header_count_out, u32 &right_header_count_out)
        {
            u32 mid = CopyUpperHalf(OffsetKey(data), OffsetKey(right_data), count);

            CopyUpperHalf(OffsetRef(data), OffsetRef(right_data), count);

            sentinel_out = GetKeyAt(right_data, 0);

            new_header_count_out = mid;
            right_header_count_out = count - mid;

            if (key < sentinel_out)
            {
                Insert(data, new_header_count_out, key, value);
                new_header_count_out++;
            }
            else
            {
                Insert(right_data, right_header_count_out, key, value);
                right_header_count_out++;
            }
        }
    };
};