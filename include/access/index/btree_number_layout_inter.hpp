#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

namespace db7::access
{
    template <typename T>
    class BtreeNumberLayoutIntermediate
    {
        static_assert(std::is_arithmetic_v<T>, "T must be a numeric type");

    private:
        u64 key_offset_;
        u64 ref_offset_;
        u64 max_count_;

        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }

        u32 GetIdx(const T *data, const u32 count, const T value)
        {
            DB7_ASSERT(count != 0, "zero count node");
            u32 lo = 0, hi = count;
            while (lo < hi)
            {
                u32 mid = lo + (hi - lo) / 2;
                if (data[mid] <= value)
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
            std::memcpy(to, from + mid + 1, (count - mid) * sizeof(Typ));
            return mid;
        }

        T *OffsetKey(byte *data)
        {
            return reinterpret_cast<T *>(data + key_offset_);
        }

        page_id *OffsetRef(byte *data)
        {
            return reinterpret_cast<page_id *>(data + ref_offset_);
        }

        T GetKeyAt(byte *data, u32 idx)
        {
            T *arr = OffsetKey(data);
            return arr[idx];
        }

        BtreeHeader<T> *CastHeader(byte *data)
        {
            return reinterpret_cast<BtreeHeader<T> *>(data);
        }

    public:
        static constexpr T UNDEFINED = std::numeric_limits<T>::max();

        BtreeNumberLayoutIntermediate(u64 header_size)
        {
            constexpr u64 pad_keys = sizeof(page_id) - 1;
            key_offset_ = shared::AlignUp(header_size, (u64)sizeof(T));
            max_count_ = (PAGE_SIZE - key_offset_ - pad_keys - sizeof(page_id)) / (sizeof(T) + sizeof(page_id));
            ref_offset_ = shared::AlignUp(key_offset_ + max_count_ * sizeof(T), (u64)sizeof(page_id));
        }

        auto Get(byte *data, const u32 count, const T value)
        {
            T *arr = OffsetKey(data);
            u32 idx = GetIdx(arr, count, value);
            return OffsetRef(data)[idx];
        }

        void Insert(byte *data, u32 count, T key, page_id value)
        {
            u32 idx = GetIdx(OffsetKey(data), count, key);
            ShiftRightInsert(OffsetKey(data), count, idx, key);
            ShiftRightInsert(OffsetRef(data), count + 1, idx + 1, value);
        }

        bool HasSpace(BtreeHeader<T> *header, T key)
        {
            (void)key;
            return header->count < max_count_;
        }

        void CreateRoot(byte *data, T key, page_id pid, page_id new_pid)
        {
            *OffsetKey(data) = key;
            *OffsetRef(data) = pid;
            *OffsetRef(data + sizeof(page_id)) = new_pid;
        }

        T Split(byte *left_data, byte *right_data, page_id new_pid, T key, page_id value)
        {
            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            u32 mid = CopyUpperHalf(OffsetKey(left_data), OffsetKey(right_data), left_header->count);

            CopyUpperHalf(OffsetRef(left_data), OffsetRef(right_data), left_header->count);

            T sentinel = GetKeyAt(left_data, mid);

            u32 left_header_count = mid;
            u32 right_header_count = left_header->count - mid - 1;

            if (key < sentinel)
            {
                Insert(left_data, left_header_count, key, value);
                left_header_count++;
            }
            else
            {
                Insert(right_data, right_header_count, key, value);
                right_header_count++;
            }

            right_header->rlink = left_header->rlink;
            right_header->count = right_header_count;
            right_header->level = left_header->level;
            right_header->max_val = left_header->max_val;

            left_header->rlink = new_pid;
            left_header->count = left_header_count;
            left_header->max_val = sentinel;

            return sentinel;
        }
    };
};