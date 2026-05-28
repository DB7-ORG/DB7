#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
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

    private:
        u64 key_offset_;

        VarlenHeader *CastHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data);
        }

        void WriteHeader(VarlenHeader *header, u64 rlink, u32 count, u8 level, u64 max_val)
        {
            header->rlink = rlink;
            header->count = count;
            header->level = level;
            header->max_val = max_val;
        }

        void WriteHeader(byte *data, u64 rlink, u32 count, u8 level, u64 max_val)
        {
            auto *header = CastHeader(data);
            WriteHeader(header, rlink, count, level, max_val);
        }

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

        u32 GetIdx(byte *data, const u32 count, const Key key)
        {
            Slot *slots = OffsetHeader(data);
            u32 lo = 0, hi = count;
            while (lo < hi)
            {
                u32 mid = lo + (hi - lo) / 2;
                int res = Cmp(ReadSlot(data, slots[mid]), key);
                if (res <= 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        void UpdateHeapSize(byte *data, u32 val)
        {
            DB7_ASSERT(val < PAGE_SIZE, "corrupted val");
            VarlenHeader *hdr = CastHeader(data);
            hdr->heap_size = val;
        }

        /* Insert to heap */
        u32 AppendHeap(byte *data, Key key, R value)
        {
            u32 heap_size = CastHeader(data)->heap_size;
            u32 off = shared::AlignDown(PAGE_SIZE - heap_size - key.len - sizeof(SlotValHeader<R>), alignof(SlotValHeader<R>));
            DB7_ASSERT(heap_size < PAGE_SIZE, "heap overflow");
            DB7_ASSERT(off > 0 && off < PAGE_SIZE, "heap overflow");

            SlotValHeader<R> *hdr = CastSlotHeader(data + off);
            hdr->result = value;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeader<R>), key.data, key.len);

            UpdateHeapSize(data, PAGE_SIZE - off);

            return off;
        }

        u32 FindSplitPoint(byte *data, Slot *slots, u32 count)
        {
            VarlenHeader *var_hdr = CastHeader(data);
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

            SlotVal val = CastSlot(ReadSlot(from, from_slots[start]));
            to_slots[0] = Slot{AppendHeap(to, Key{0, nullptr}, val.hdr.result)};

            for (u32 i = start + 1; i < end; i++)
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

        R ReadMaxVal(VarlenHeader *header)
        {
            return header->max_val;
        }

        Key DeepCopy(Key key)
        {
            byte *sentinel_copy = new byte[key.len];
            std::memcpy(sentinel_copy, key.data, key.len);
            return Key{key.len, sentinel_copy};
        }

        void InsertInternal(byte *data, u32 count, Key key, R value)
        {
            Slot *slots = OffsetHeader(data);

            /* Insert to heap */
            u32 off = AppendHeap(data, key, value);

            /* Insert slot */
            u32 idx = GetIdx(data, count, key);
            Slot slot = Slot{off};
            ShiftRightInsert(slots, count, idx, slot);
        }

    public:
        static constexpr R UNDEFINED = std::numeric_limits<R>::max();

        BtreeVarlenLayoutIntermediate() : key_offset_(sizeof(VarlenHeader)) {}

        R Get(byte *data, const u32 count, const Key key)
        {
            Slot *slots = OffsetHeader(data);
            u32 idx = GetIdx(data, count, key);

            DB7_ASSERT(idx > 0, "key routes before leftmost entry");
            SlotVal val = CastSlot(ReadSlot(data, slots[idx - 1]));
            return val.hdr.result;
        }

        void Insert(byte *data, Key key, R value)
        {
            u32 count = CastHeader(data)->count;
            InsertInternal(data, count, key, value);
            CastHeader(data)->count++;

            delete[] key.data;
        }

        bool HasSpace(byte *data, Key key)
        {
            auto *hdr = CastHeader(data); // TODO fix this, this can all fit into taken_space
            return key_offset_ + hdr->count * sizeof(Slot) + hdr->heap_size + CalcWorstCaseSize(key) < PAGE_SIZE;
        }

        bool HasSplit(byte *data, Key key)
        {
            auto *header = CastHeader(data);
            if (ReadMaxVal(header) == UNDEFINED)
                return false; // rightmost page, no high key
            auto *slot = ReadSlot((byte *)header, header->max_val);
            if (header->level > 0)
            {
                return Cmp(slot, key) <= 0;
            }

            auto hdr = *reinterpret_cast<SlotValHeader<u64> *>(slot);
            SlotVal<u64> val = SlotVal{hdr, static_cast<byte *>(slot) + sizeof(hdr)};

            u16 len = val.hdr.len;
            u32 min_len = std::min(key.len, len);
            int cmp = std::memcmp(val.data, key.data, min_len);
            if (cmp != 0)
                return cmp <= 0;
            return (key.len < len) - (key.len > len) <= 0;
        }

        Key Split(byte *left_data, byte *right_data, page_id new_pid, Key key, R value)
        {
            UpdateHeapSize(right_data, 0);

            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            u32 mid = CopyUpperHalf(left_data, right_data, left_header->count);

            Slot *left_slots = OffsetHeader(left_data);
            u64 right_max = UNDEFINED;
            if (ReadMaxVal(left_header) != UNDEFINED)
            {
                SlotVal val = CastSlot(ReadSlot(left_data, left_header->max_val));
                right_max = (u64)AppendHeap(right_data, Key{val.hdr.len, val.data}, val.hdr.result);
            }

            Key sentinel = DeepCopy(ReadKey(left_data, OffsetHeader(left_data)[mid]));

            CompactHeap(left_data, left_slots, mid);

            u64 left_max = (u64)AppendHeap(left_data, sentinel, UNDEFINED);

            u32 left_header_count = mid;
            u32 right_header_count = left_header->count - mid;

            if (Cmp(sentinel.data, key) > 0)
            {
                InsertInternal(left_data, left_header_count++, key, value);
            }
            else
            {
                InsertInternal(right_data, right_header_count++, key, value);
            }

            WriteHeader(right_header, left_header->rlink, right_header_count, left_header->level, right_max);

            WriteHeader(left_header, new_pid, left_header_count, left_header->level, left_max);

            return sentinel;
        }

        void CreateRoot(byte *data, Key key, page_id pid, page_id new_pid)
        {
            UpdateHeapSize(data, 0);

            Slot *slots = OffsetHeader(data);

            slots[0] = Slot{AppendHeap(data, Key{0, nullptr}, pid)};

            slots[1] = Slot{AppendHeap(data, key, new_pid)};

            CastHeader(data)->count = 2;

            delete[] key.data;
        }

        void InitHeader(byte *data, u32 count, u8 level)
        {
            WriteHeader(data, UNDEFINED, count, level, UNDEFINED);
        }

        u64 GetRLink(byte *data)
        {
            return CastHeader(data)->rlink;
        }
    };
}