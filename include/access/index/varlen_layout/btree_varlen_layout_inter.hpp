#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"
#include "debug/printer.hpp"
#include "access/index/varlen_layout/btree_varlen_models.hpp"

#include <span>
#include <limits>
#include <queue>
#include <algorithm>

namespace db7::access
{

    class BtreeVarlenLayoutIntermediate
    {
        using R = page_id;

        u64 header_size_;
        u64 key_offset_;

    private:
        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }
        /**
         * negative → slot < key
         * zero → slot == key
         * positive → slot > key
         */
        int Cmp(byte *slot, Key key)
        {
            SlotVal<R> val = CastSlot(slot);
            u16 len = val.hdr.len;
            u32 min_len = std::min(key.len, len);
            int cmp = std::memcmp(val.data, key.data, min_len);
            if (cmp != 0)
                return cmp;
            return (key.len < len) - (key.len > len);
        }

        byte *ReadSlot(byte *data, Slot slot)
        {
            return data + slot.offset;
        }

        byte *ReadSlot(byte *data, u32 offset)
        {
            return data + offset;
        }

        SlotValHeader<R> *CastSlotHeader(void *slot)
        {
            return reinterpret_cast<SlotValHeader<R> *>(slot);
        }

        SlotVal<R> CastSlot(void *slot)
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
            return key.len + sizeof(SlotValHeader<R>) + alignof(SlotValHeader<R>);
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
        u32 AppendHeap(byte *data, Key key, R value)
        {
            u32 heap_size = GetVarlenHeader(data)->heap_size;
            u32 off = shared::AlignDown(PAGE_SIZE - heap_size - key.len - sizeof(SlotValHeader<R>), alignof(SlotValHeader<R>));
            SlotValHeader<R> *hdr = CastSlotHeader(data + off);
            hdr->result = value;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeader<R>), key.data, key.len);
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
                SlotValHeader<R> *hdr = CastSlotHeader(ReadSlot(data, slots[i]));
                u32 entry_size = shared::AlignUp(
                    (u32)(sizeof(SlotValHeader<R>) + hdr->len),
                    (u32)alignof(SlotValHeader<R>));
                accumulated += entry_size;
                if (accumulated >= target)
                    return i + 1;
            }

            DB7_UNREACHABLE();
        }

        void CopyRange(byte *from, byte *to, Slot *from_slots, u32 start, u32 end)
        {
            auto *to_slots = OffsetHeader(to);
            for (u32 i = start; i < end; i++)
            {
                SlotVal val = CastSlot(ReadSlot(from, from_slots[i]));
                u32 off = AppendHeap(to, Key{val.hdr.len, val.data}, val.hdr.result);
                to_slots[i - start] = Slot{off};
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
                SlotValHeader<R> *hdr = CastSlotHeader(src);
                u32 entry_size = sizeof(SlotValHeader<R>) + hdr->len;
                u32 off = shared::AlignDown(PAGE_SIZE - write_pos - entry_size, alignof(SlotValHeader<R>));

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

            return split;
        }

        BtreeHeader<u32> *CastHeader(byte *data)
        {
            return reinterpret_cast<BtreeHeader<u32> *>(data);
        }

        Key ReadKey(byte *data, Slot slot)
        {
            byte *ptr = ReadSlot(data, slot);
            SlotValHeader<R> *hdr = CastSlotHeader(ptr);
            return Key{hdr->len, ptr + sizeof(SlotValHeader<R>)};
        }

        Key ReadKey(byte *data, u32 offset)
        {
            return ReadKey(data, Slot{offset});
        }

    public:
        static constexpr R UNDEFINED = std::numeric_limits<R>::max();

        BtreeVarlenLayoutIntermediate(u64 header_size) : header_size_(header_size), key_offset_(header_size + sizeof(VarlenHeader)) {}

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

            /* Insert to heap */
            u32 off = AppendHeap(data, key, value);

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

        bool HasSplit(BtreeHeader<u32> *header, Key key)
        {
            if (header->max_val == (u32)UNDEFINED)
                return false; // rightmost page, no high key
            return Cmp(ReadSlot((byte *)header, header->max_val), key) >= 0;
        }

        T Split(byte *left_data, byte *right_data, page_id new_pid, Key key, R value)
        {
            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            u32 mid = CopyUpperHalf(left_data, right_data, left_header->count);

            Slot *left_slots = OffsetHeader(left_data);
            if (left_header->max_val != (u32)UNDEFINED)
            {
                SlotVal val = CastSlot(ReadSlot(left_data, left_header->max_val));
                right_header->max_val = AppendHeap(right_data, Key{val.hdr.len, val.data}, val.hdr.result);
            }

            CompactHeap(left_data, left_slots, mid);

            Key sentinel = ReadKey(right_data, OffsetHeader(right_data)[0]);
            u32 left_max = AppendHeap(left_data, sentinel, UNDEFINED);

            u32 left_header_count = mid;
            u32 right_header_count = left_header->count - mid;
            byte *sep = ReadSlot(right_data, OffsetHeader(right_data)[0]);
            if (Cmp(sep, key) > 0)
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
            // right_header->max_val = right_max;

            left_header->rlink = new_pid;
            left_header->count = left_header_count;
            left_header->max_val = left_max;

            // shared::PrintVarlenLayout(left_data);

            // shared::PrintVarlenLayout(right_data);

            return sentinel;
        }
    };
}