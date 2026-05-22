#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_header.hpp"

#include <span>
#include <limits>

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
        u32 taken_space; // taken heap space
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

        int Cmp(byte *data, byte *slot, Key key)
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

        SlotVal CastSlot(byte *slot)
        {
            auto hdr = *reinterpret_cast<SlotValHeader *>(slot);
            return SlotVal{hdr, slot + sizeof(hdr)};
        }

        Slot *OffsetHeader(byte *data)
        {
            Slot *slots = reinterpret_cast<Slot *>(data + key_offset_);
            return slots + 1;
        }

        u32 CalcWorstCaseSize(Key key)
        {
            return key.len + sizeof(SlotValHeader) + alignof(SlotValHeader);
        }

        u32 GetIdx(byte *data, const u32 count, const Key key, bool &found)
        {
            DB7_ASSERT(count != 0, "zero count node");

            Slot *slots = OffsetHeader(data);
            u32 lo = 0, hi = count;
            while (lo < hi)
            {
                u32 mid = lo + (hi - lo) / 2;
                int res = Cmp(key.data, ReadSlot(data, slots[mid]), key);
                if (res == 0)
                {
                    found = true;
                    return mid;
                }
                if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        VarlenHeader *GetHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data + header_size_);
        }

        void UpdateFreeSpace(byte *data, u32 val)
        {
            auto hdr = GetHeader(data);
            hdr->taken_space += val;
        }

    public:
        BtreeVarlenLayoutLeaf(u64 header_size) : header_size_(header_size), key_offset_(header_size + sizeof(u32)) {}

        R Get(byte *data, const u32 count, const Key key)
        {
            Slot *slots = OffsetHeader(data);
            bool found;
            u32 idx = GetIdx(data, count, key, found);
            if (found)
            {
                SlotVal val = CastSlot(data + slots[idx].offset);
                return val.hdr.result;
            }
            return UNDEFINED;
        }

        void Insert(byte *data, u32 count, Key key, R value)
        {
            Slot *slots = OffsetHeader(data);

            if (count == 0)
            {
                u32 off = shared::AlignDown(PAGE_SIZE - key.len - sizeof(SlotValHeader), alignof(SlotValHeader));
                byte *dest = data + off;
                SlotValHeader *hdr = reinterpret_cast<SlotValHeader *>(dest);
                hdr->result = value;
                hdr->len = key.len;
                std::memcpy(data + off, key.data, key.len);

                /* Insert slot */
                bool found;
                u32 idx = 0;
                Slot slot = Slot{off};
                ShiftRightInsert(slots, count, idx, slot);
            }
            else
            {
                /* Insert to heap */
                u32 off = shared::AlignDown(slots[count - 1].offset - key.len - sizeof(SlotValHeader), alignof(SlotValHeader));
                byte *dest = data + off;
                SlotValHeader *hdr = reinterpret_cast<SlotValHeader *>(dest);
                hdr->result = value;
                hdr->len = key.len;
                std::memcpy(data + off, key.data, key.len);

                /* Insert slot */
                bool found;
                u32 idx = GetIdx(data, count, key, found);
                Slot slot = Slot{off};
                ShiftRightInsert(slots, count, idx, slot);
            }
        }

        template <typename Typ>
        u32 CopyUpperHalf(Typ *from, Typ *to, u32 count)
        {
        }

        bool HasSpace(BtreeHeader<u8> *header, Key key)
        {
            auto hdr = GetHeader(reinterpret_cast<byte *>(header)); // TODO fix this, this can all fit into taken_space
            return header_size_ + header->count * sizeof(Slot) + hdr->taken_space + CalcWorstCaseSize(key) < PAGE_SIZE;
        }

        void CreateRoot(byte *data, Key key, page_id pid, page_id new_pid)
        {
        }

        void Split(byte *data, byte *right_data, u32 count, Key key, page_id value, Key &sentinel_out, u32 &new_header_count_out, u32 &right_header_count_out)
        {
        }
    };
}