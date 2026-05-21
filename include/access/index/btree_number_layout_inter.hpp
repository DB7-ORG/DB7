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
        using R = u64;

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

    public:
        BtreeNumberLayoutIntermediate(u64 key_offset, u64 ref_offset, u64 max_count)
            : key_offset_(key_offset), ref_offset_(ref_offset), max_count_(max_count) {}

        auto Get(byte *data, const u32 count, const T value)
        {
            T *arr = reinterpret_cast<T *>(data + key_offset_);
            u32 idx = GetIdx(arr, count, value);
            return reinterpret_cast<page_id *>(data + ref_offset_)[idx];
        }

        void Insert(byte *data, u32 count, T key, page_id value)
        {
            u32 idx = GetIdx((T *)(data + key_offset_), count, key);
            ShiftRightInsert((T *)(data + key_offset_), count, idx, key);
            ShiftRightInsert((page_id *)(data + ref_offset_), count + 1, idx + 1, value);
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
            *(T *)(data + key_offset_) = key;
            *(page_id *)(data + ref_offset_) = pid;
            *(page_id *)(data + ref_offset_ + sizeof(page_id)) = new_pid;
        }
    };
}