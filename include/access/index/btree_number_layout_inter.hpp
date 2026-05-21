#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

namespace db7::access
{
    class BtreeNumberLayoutIntermediate
    {
    private:
        using T = u64;
        using R = page_id;

        static constexpr u64 UNDEFINED = 0;

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

        T *OffsetKey(byte *data)
        {
            return reinterpret_cast<T *>(data + key_offset_);
        }

        page_id *OffsetRef(byte *data)
        {
            return reinterpret_cast<page_id *>(data + ref_offset_);
        }

    public:
        BtreeNumberLayoutIntermediate(u64 key_offset, u64 ref_offset, u64 max_count)
            : key_offset_(key_offset), ref_offset_(ref_offset), max_count_(max_count) {}

        auto Get(byte *data, const u32 count, const T value)
        {
            T *arr = OffsetKey(data);
            u32 idx = GetIdx(arr, count, value);
            return OffsetRef(data)[idx];
        }

        T GetKeyAt(byte *data, u32 idx)
        {
            T *arr = OffsetKey(data);
            return arr[idx];
        }

        void Insert(byte *data, u32 count, T key, page_id value)
        {
            u32 idx = GetIdx(OffsetKey(data), count, key);
            ShiftRightInsert(OffsetKey(data), count, idx, key);
            ShiftRightInsert(OffsetRef(data), count + 1, idx + 1, value);
        }

        template <typename Typ>
        u32 CopyUpperHalf(Typ *from, Typ *to, u32 count)
        {
            u32 mid = (count + 1) / 2;
            std::memcpy(to, from + mid + 1, (count - mid) * sizeof(Typ));
            return mid;
        }

        bool HasSpace(BtreeHeader *header)
        {
            return header->count < max_count_;
        }

        void CreateRoot(byte *data, T key, page_id pid, page_id new_pid)
        {
            *OffsetKey(data) = key;
            *OffsetRef(data) = pid;
            *OffsetRef(data + sizeof(page_id)) = new_pid;
        }

        void Split(byte *data, byte *right_data, u32 count, T key, page_id value, T &sentinel_out, u32 &new_header_count_out, u32 &right_header_count_out)
        {
            u32 mid = CopyUpperHalf(OffsetKey(data), OffsetKey(right_data), count);

            CopyUpperHalf(OffsetRef(data), OffsetRef(right_data), count);

            sentinel_out = GetKeyAt(data, mid);

            new_header_count_out = mid;
            right_header_count_out = count - mid - 1;

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
}