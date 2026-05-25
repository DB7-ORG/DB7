#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

#include <span>
#include <limits>
#include <queue>
#include <algorithm>

#define START_taken_space = PAGE_SIZE - sizeof(BtreeHeader);

namespace db7::access
{
    struct Key
    {
        u16 len;
        byte *data;
    };

    struct VarlenHeader
    {
        u32 heap_size; // taken heap space
        // u32 total_taken; // heap_size + taken slot size + headers //TODO optimization for Free space calc
    };

    struct Slot
    {
        u32 offset;
    };

    struct SlotValHeader
    {
        u64 result;
        u16 len;
    };

    struct SlotVal
    {
        SlotValHeader hdr;
        byte *data;
    };

    class BtreeVarlenLayoutLeaf
    {
        using R = u64;
        static constexpr R UNDEFINED = std::numeric_limits<R>::max();

        u64 header_size_;
        u64 key_offset_;

    private:
        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }

        int Cmp(byte *slot, Key key)
        {
            SlotVal val = CastSlot(slot);
            u16 len = val.hdr.len;
            u32 min_len = std::min(key.len, len);
            int cmp = std::memcmp(key.data, val.data, min_len);
            if (cmp != 0)
                return cmp;
            return (key.len > len) - (key.len < len);
        }

        byte *ReadSlot(byte *data, Slot slot)
        {
            return data + slot.offset;
        }

        byte *ReadSlot(byte *data, u32 offset)
        {
            return data + offset;
        }

        SlotValHeader *CastSlotHeader(void *slot)
        {
            return reinterpret_cast<SlotValHeader *>(slot);
        }

        SlotVal CastSlot(void *slot)
        {
            auto hdr = *CastSlotHeader(slot);
            return SlotVal{hdr, static_cast<byte *>(slot) + sizeof(hdr)};
        }

        Slot *OffsetHeader(byte *data)
        {
            return reinterpret_cast<Slot *>(data + key_offset_);
        }

        u32 CalcWorstCaseSize(Key key)
        {
            return key.len + sizeof(SlotValHeader) + alignof(SlotValHeader);
        }

        u32 GetIdx(byte *data, const u32 count, const Key key, bool &found)
        {
            Slot *slots = OffsetHeader(data);
            u32 lo = 0, hi = count;
            while (lo < hi)
            {
                u32 mid = lo + (hi - lo) / 2;
                int res = Cmp(ReadSlot(data, slots[mid]), key);
                if (res == 0)
                {
                    found = true;
                    return mid;
                }
                else if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        VarlenHeader *GetVarlenHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data + header_size_);
        }

        void UpdateHeapSize(byte *data, u32 val)
        {
            VarlenHeader *hdr = GetVarlenHeader(data);
            hdr->heap_size = val;
        }

        /* Insert to heap */
        u32 AppendHeap(byte *data, u32 heap_size, Key key, R value)
        {
            u32 off = shared::AlignDown(PAGE_SIZE - heap_size - key.len - sizeof(SlotValHeader), alignof(SlotValHeader));
            SlotValHeader *hdr = CastSlotHeader(data + off);
            hdr->result = value;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeader), key.data, key.len);
            UpdateHeapSize(data, PAGE_SIZE - off);
            return off;
        }

        u32 FindSplitPoint(byte *data, Slot *slots, u32 count)
        {
            VarlenHeader *var_hdr = GetVarlenHeader(data);
            u32 target = var_hdr->heap_size / 2;
            u32 accumulated = 0;

            for (u32 i = 0; i < count; i++)
            {
                SlotValHeader *hdr = CastSlotHeader(ReadSlot(data, slots[i]));
                u32 entry_size = shared::AlignUp(
                    (u32)(sizeof(SlotValHeader) + hdr->len),
                    (u32)alignof(SlotValHeader));
                accumulated += entry_size;
                if (accumulated >= target)
                    return i + 1;
            }

            DB7_UNREACHABLE();
        }

        void AppendSorted(byte *data, u32 pos, Key key, R value)
        {
            Slot *slots = OffsetHeader(data);
            u32 free_end = (pos == 0) ? PAGE_SIZE : slots[pos - 1].offset;
            u32 off = shared::AlignDown(
                free_end - key.len - sizeof(SlotValHeader),
                alignof(SlotValHeader));

            byte *dest = data + off;
            CastSlotHeader(dest)->result = value;
            CastSlotHeader(dest)->len = key.len;
            std::memcpy(dest + sizeof(SlotValHeader), key.data, key.len);

            slots[pos] = Slot{off};
        }

        void CopyRange(byte *from, byte *to, Slot *from_slots, u32 start, u32 end)
        {
            for (u32 i = start; i < end; i++)
            {
                SlotVal val = CastSlot(ReadSlot(from, from_slots[i]));
                AppendSorted(to, i - start, Key{val.hdr.len, val.data}, val.hdr.result);
            }
        }

        void CompactHeap(byte *data, Slot *slots, u32 count)
        {
            // sort slot indices by offset descending (highest first = end of page)
            u32 indices[count];
            for (u32 i = 0; i < count; i++)
                indices[i] = i;

            std::sort(indices, indices + count, [&](u32 a, u32 b)
                      { return slots[a].offset > slots[b].offset; });

            u32 write_pos = 0;
            for (u32 i = 0; i < count; i++)
            {
                u32 idx = indices[i];
                byte *src = ReadSlot(data, slots[idx]);
                SlotValHeader *hdr = CastSlotHeader(src);
                u32 entry_size = sizeof(SlotValHeader) + hdr->len;
                u32 off = shared::AlignDown(PAGE_SIZE - write_pos - entry_size, alignof(SlotValHeader));

                std::memmove(data + off, src, entry_size);
                slots[idx].offset = off;
                write_pos = PAGE_SIZE - off;
            }

            UpdateHeapSize(data, write_pos);
        }

        u32 CopyUpperHalf(byte *from, byte *to, u32 count)
        {
            DB7_ASSERT(count != 0, "nothing to copy");

            Slot *from_slots = OffsetHeader(from);

            u32 split = FindSplitPoint(from, from_slots, count);

            DB7_ASSERT(split > 0 && split < count, "split must leave tuples on both sides");

            CopyRange(from, to, from_slots, split, count);

            CompactHeap(from, from_slots, split);

            return split;
        }

        BtreeHeader<u32> *CastHeader(byte *data)
        {
            return reinterpret_cast<BtreeHeader<u32> *>(data);
        }

        Key ReadKey(byte *data, Slot slot)
        {
            byte *ptr = ReadSlot(data, slot);
            SlotValHeader *hdr = CastSlotHeader(ptr);
            return Key{hdr->len, ptr + sizeof(SlotValHeader)};
        }

        Key ReadKey(byte *data, u32 offset)
        {
            return ReadKey(data, Slot{offset});
        }

    public:
        BtreeVarlenLayoutLeaf(u64 header_size) : header_size_(header_size), key_offset_(header_size + sizeof(VarlenHeader)) {}

        R Get(byte *data, const u32 count, const Key key)
        {
            Slot *slots = OffsetHeader(data);
            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            if (found)
            {
                SlotVal val = CastSlot(ReadSlot(data, slots[idx]));
                return val.hdr.result;
            }
            return UNDEFINED;
        }

        void Insert(byte *data, u32 count, Key key, R value)
        {
            Slot *slots = OffsetHeader(data);

            auto var_hdr = GetVarlenHeader(data);

            /* Insert to heap */
            u32 off = AppendHeap(data, var_hdr->heap_size, key, value);

            /* Insert slot */
            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            Slot slot = Slot{off};
            ShiftRightInsert(slots, count, idx, slot);
        }

        bool HasSpace(BtreeHeader<u32> *header, Key key)
        {
            auto hdr = GetVarlenHeader(reinterpret_cast<byte *>(header)); // TODO fix this, this can all fit into taken_space
            return key_offset_ + header->count * sizeof(Slot) + hdr->heap_size + CalcWorstCaseSize(key) < PAGE_SIZE;
        }

        void Split(byte *left_data, byte *right_data, page_id new_pid, Key key, R value)
        {
            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            u32 mid = CopyUpperHalf(left_data, right_data, left_header->count);

            u32 right_heap_size = GetVarlenHeader(right_data)->heap_size;
            Key old_high = ReadKey(left_data, left_header->max_val);
            u32 right_max = AppendHeap(right_data, right_heap_size, old_high, UNDEFINED);

            u32 left_heap_size = GetVarlenHeader(left_data)->heap_size;
            Key sentinel = ReadKey(right_data, OffsetHeader(right_data)[0]);
            u32 left_max = AppendHeap(left_data, left_heap_size, sentinel, UNDEFINED);

            u32 left_header_count = mid;
            u32 right_header_count = left_header->count - mid;
            byte *sep = ReadSlot(right_data, OffsetHeader(right_data)[0]);
            if (Cmp(sep, key) < 0)
            {
                Insert(left_data, left_header_count++, key, value);
            }
            else
            {
                Insert(right_data, right_header_count++, key, value);
            }

            right_header->rlink = left_header->rlink;
            right_header->count = right_header_count;
            right_header->level = left_header->level;
            right_header->max_val = right_max;

            left_header->rlink = new_pid;
            left_header->count = left_header_count;
            left_header->max_val = left_max;
        }
    };
}