#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

#include <span>
#include <limits>
#include <queue>

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

        SlotVal CastSlot(byte *slot)
        {
            auto hdr = *reinterpret_cast<SlotValHeader *>(slot);
            return SlotVal{hdr, slot + sizeof(hdr)};
        }

        SlotVal CastSlot(Slot *slot)
        {
            auto hdr = *reinterpret_cast<SlotValHeader *>(slot);
            return SlotVal{hdr, reinterpret_cast<byte *>(slot) + sizeof(hdr)};
        }

        Slot *OffsetHeader(byte *data)
        {
            Slot *slots = reinterpret_cast<Slot *>(data + key_offset_);
            return slots;
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
            auto hdr = GetVarlenHeader(data);
            hdr->heap_size = val;
        }

        u32 AppendHeap(byte *data, u32 heap_size, Key key, R value)
        {
            /* Insert to heap */
            u32 off = shared::AlignDown(PAGE_SIZE - heap_size - key.len - sizeof(SlotValHeader), alignof(SlotValHeader));
            byte *dest = data + off;
            SlotValHeader *hdr = reinterpret_cast<SlotValHeader *>(dest);
            hdr->result = value;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeader), key.data, key.len);
            return off;
        }

        u32 CopyUpperHalf(byte *from, byte *to, u32 count)
        {
            Slot *from_slots = OffsetHeader(from);
            auto var_hdr = GetVarlenHeader(from);
            u32 total_heap = var_hdr->heap_size;
            u32 target = total_heap / 2;

            // Find split point by accumulated byte size
            u32 accumulated = 0;
            u32 split = 0;
            for (u32 i = 0; i < count; i++)
            {
                SlotVal val = CastSlot(ReadSlot(from, from_slots[i]));
                u32 entry_size = sizeof(SlotValHeader) + val.hdr.len;
                entry_size = shared::AlignUp(entry_size, (u32)alignof(SlotValHeader));
                accumulated += entry_size;
                if (accumulated >= target)
                {
                    split = i + 1; // this entry stays on left, split after it
                    break;
                }
            }

            DB7_ASSERT(split != 0, "Tuple size must be so atleast 2 tuples can fit into a single page");
            DB7_ASSERT(split < count, "Tuple size must be so atleast 2 tuples can fit into a single page");

            // Copy right half (split..count-1) into 'to' page
            for (u32 i = split; i < count; i++)
            {
                SlotVal val = CastSlot(ReadSlot(from, from_slots[i]));
                Key key = Key{val.hdr.len, val.data};
                Insert(to, i - split, key, val.hdr.result);
            }

            // Compact left half using a priprity queue.
            std::priority_queue<std::pair<u32, u32>> pq; // {offset, slot_index}
            for (u32 i = 0; i < split; i++)
            {
                pq.push({from_slots[i].offset, i});
            }

            u32 new_heap_size = 0;
            while (!pq.empty())
            {
                auto [old_off, slot_idx] = pq.top();
                pq.pop();

                byte *slot_ptr = ReadSlot(from, Slot{old_off});
                SlotVal val = CastSlot(slot_ptr);
                u32 entry_size = sizeof(SlotValHeader) + val.hdr.len;
                u32 off = shared::AlignDown(PAGE_SIZE - new_heap_size - entry_size, alignof(SlotValHeader));

                std::memmove(from + off, slot_ptr, entry_size);
                from_slots[slot_idx].offset = off;
                new_heap_size = PAGE_SIZE - off;
            }

            UpdateHeapSize(from, new_heap_size);

            return split;
        }

        BtreeHeader<u32> *CastHeader(byte *data)
        {
            return reinterpret_cast<BtreeHeader<u32> *>(data);
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

            UpdateHeapSize(data, PAGE_SIZE - off);
        }

        bool HasSpace(BtreeHeader<u32> *header, Key key)
        {
            auto hdr = GetVarlenHeader(reinterpret_cast<byte *>(header)); // TODO fix this, this can all fit into taken_space
            return key_offset_ + header->count * sizeof(Slot) + hdr->heap_size + CalcWorstCaseSize(key) < PAGE_SIZE;
        }

        // TODO refactor this
        void Split(byte *left_data, byte *right_data, page_id new_pid, Key key, R value)
        {
            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            u32 max_val = left_header->max_val;
            byte *slot_ptr = ReadSlot(left_data, max_val);
            auto hdr = CastSlot(slot_ptr);
            auto key = Key{hdr.hdr.len, slot_ptr};
            u32 right_max_val = AppendHeap(right_data, 0, key, hdr.hdr.result);

            u32 mid = CopyUpperHalf(left_data, right_data, left_header->count);

            Slot *slots = OffsetHeader(right_data);
            slot_ptr = ReadSlot(left_data, max_val);
            auto hdr = CastSlot(slot_ptr);
            auto key = Key{hdr.hdr.len, slot_ptr};
            u32 left_max_val = AppendHeap(right_data, 0, key, hdr.hdr.result);

            u32 left_header_count = mid;
            u32 right_header_count = left_header->count - mid;
            if (Cmp(slot_ptr, key) < 0)
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
            right_header->max_val = right_max_val;

            left_header->rlink = new_pid;
            left_header->count = left_header_count;
            left_header->max_val = left_max_val;
        }
    };
}