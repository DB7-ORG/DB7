#pragma once

#include "storage/storage_common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"
#include "debug/printer.hpp"
#include "access/index/varlen_layout/btree_varlen_models.hpp"
#include "shared/key_encode_util.hpp"

#include <span>
#include <limits>
#include <queue>
#include <algorithm>

namespace db7::access
{

    class BtreeVarlenLayoutLeaf
    {
        using R = u64;

    private:
        u64 key_offset_;

        VarlenHeader *CastHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data);
        }

        void WriteHeader(VarlenHeader *header, page_id pid, u64 rlink, u64 llink, u32 count, u8 level, u64 max_val, u32 prefix_offset, u16 prefix_len)
        {
            header->pid = pid;
            header->rlink = rlink;
            header->llink = llink;
            header->count = count;
            header->level = level;
            header->max_val = max_val;

            header->prefix_offset = prefix_offset;
            header->prefix_len = prefix_len;
        }

        void WriteHeader(byte *data, page_id pid, u64 rlink, u64 llink, u32 count, u8 level, u64 max_val, u32 prefix_offset, u16 prefix_len)
        {
            auto *header = CastHeader(data);
            WriteHeader(header, pid, rlink, llink, count, level, max_val, prefix_offset, prefix_len);
        }

        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }

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

        int Cmp(Key slot, Key key)
        {
            u32 min_len = std::min(key.len, slot.len);
            int cmp = std::memcmp(slot.data, key.data, min_len);
            if (cmp != 0)
                return cmp;
            return (key.len < slot.len) - (key.len > slot.len);
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

        void UpdateHeapSize(byte *data, u32 val)
        {
            VarlenHeader *hdr = CastHeader(data);
            hdr->heap_size = val;
        }

        void UpdatePrefix(byte *data, u32 prefix_offset, u16 prefix_len)
        {
            VarlenHeader *hdr = CastHeader(data);
            hdr->prefix_offset = prefix_offset;
            hdr->prefix_len = prefix_len;
        }

        u32 ReserveSlot(byte *data, u32 len)
        {
            u32 heap_size = CastHeader(data)->heap_size;
            DB7_ASSERT(heap_size < PAGE_SIZE, "heap overflow");
            u32 off = shared::AlignDown(PAGE_SIZE - heap_size - len - sizeof(SlotValHeader<R>), alignof(SlotValHeader<R>));
            UpdateHeapSize(data, PAGE_SIZE - off);
            return off;
        }

        /* Insert to heap */
        u32 AppendHeap(byte *data, Key key, R value)
        {
            u32 off = ReserveSlot(data, key.len);

            DB7_ASSERT(off > 0 && off < PAGE_SIZE, "heap overflow");

            SlotValHeader<R> *hdr = CastSlotHeader(data + off);
            hdr->result = value;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeader<R>), key.data, key.len);

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
            for (u32 i = start; i < end; i++)
            {
                SlotVal val = CastSlot(ReadSlot(from, from_slots[i]));
                u32 off = AppendHeap(to, Key{val.hdr.len, val.data}, val.hdr.result);
                to_slots[i - start] = Slot{off};
            }
        }

        void CompactHeap(byte *data, Slot *slots, u32 count, u32 prefix_len)
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
                SlotVal<R> val = CastSlot(ReadSlot(data, slots[idx]));

                u32 entry_size = sizeof(SlotValHeader<R>) + val.hdr.len - prefix_len;
                u32 off = shared::AlignDown(PAGE_SIZE - write_pos - entry_size, alignof(SlotValHeader<R>));

                std::memmove(data + off + sizeof(SlotValHeader<R>), val.data + prefix_len, val.hdr.len - prefix_len);
                auto *new_hdr = CastSlotHeader(data + off);
                new_hdr->len = val.hdr.len - prefix_len;
                new_hdr->result = val.hdr.result;

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

        Key DeepCopy(Key key)
        {
            byte *sentinel_copy = new byte[key.len];
            std::memcpy(sentinel_copy, key.data, key.len);
            return Key(key.len, sentinel_copy);
        }

        Key DeepCopyEncoded(Key key)
        {
            u32 BASE_OVERHEAD = key.len * 10; // TODO store this somewhere, in the tree for example based on schema
            byte *sentinel_copy = new byte[key.len + BASE_OVERHEAD];
            u32 size = shared::KeyNormEncoder::Encode(sentinel_copy, std::span<const byte>{key.data, key.len}, key.data == nullptr, false, false);
            return MakeEncodedKey(size, sentinel_copy);
        }

        Key PrefixNewKey(byte *data, Key key)
        {
            auto *header = CastHeader(data);
            u16 len = header->prefix_len;
            return Key{static_cast<u16>(key.len - len), key.data + len};
        }

        void InsertInternal(byte *data, u32 count, Key key, R value)
        {
            Slot *slots = OffsetHeader(data);

            /* Insert to heap */
            u32 off = AppendHeap(data, key, value);

            /* Insert slot */
            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            Slot slot = Slot{off};
            ShiftRightInsert(slots, count, idx, slot); // TODO this should increment header count
        }

        R ReadMaxVal(VarlenHeader *header)
        {
            return header->max_val;
        }

    public:
        static constexpr R UNDEFINED = std::numeric_limits<R>::max();
        static constexpr u32 UNDEFINED_OFFSET = std::numeric_limits<u32>::max();

        BtreeVarlenLayoutLeaf() : key_offset_(sizeof(VarlenHeader))
        {
        }

        R Get(byte *data, const u32 count, const Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            Key new_key = PrefixNewKey(data, key);

            Slot *slots = OffsetHeader(data);
            bool found = false;
            u32 idx = GetIdx(data, count, new_key, found);
            if (found)
            {
                SlotVal val = CastSlot(ReadSlot(data, slots[idx]));
                return val.hdr.result;
            }
            return UNDEFINED;
        }

        void Insert(byte *data, Key key, R value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            u32 count = CastHeader(data)->count;
            Key new_key = PrefixNewKey(data, key);
            InsertInternal(data, count, new_key, value);
            CastHeader(data)->count++;
        }

        bool HasSpace(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            Key new_key = PrefixNewKey(data, key);
            auto hdr = CastHeader(data); // TODO fix this, this can all fit into taken_space
            return key_offset_ + hdr->count * sizeof(Slot) + hdr->heap_size + CalcWorstCaseSize(new_key) < PAGE_SIZE;
        }

        bool HasSplit(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            // Key new_key = PrefixNewKey(data, key);
            auto *header = CastHeader(data);
            if (ReadMaxVal(header) == UNDEFINED)
                return false; // rightmost page, no high key
            auto *slot = ReadSlot((byte *)header, header->max_val);
            return Cmp(slot, key) <= 0;
        }

        Key OffsetCommonPrefix(byte *data, u32 len, u32 prefix_len)
        {
            DB7_ASSERT(len >= prefix_len, "Invalid resulting len");
            DB7_ASSERT(len - prefix_len <= std::numeric_limits<u16>::max(), "key too long");
            return Key{(u16)(len - prefix_len), data + prefix_len};
        }

        u32 CommonPrefixLen(const byte *a, const byte *b, u32 len_a, u32 len_b)
        {
            u32 i = 0;
            u32 max_len = std::min(len_a, len_b);
            while (i < max_len && a[i] == b[i])
                i++;
            return i;
        }

        SlotVal<R> GetMaxValSlot(byte *data)
        {
            auto header = CastHeader(data);
            if (header->max_val != UNDEFINED)
            {
                return CastSlot(ReadSlot(data, header->max_val));
            }
            else
            {
                return SlotVal<R>{SlotValHeader<R>{0, 0}, nullptr};
            }
        }

        SlotVal<R> GetPrefixSlot(byte *data)
        {
            auto header = CastHeader(data);
            if (header->prefix_offset != UNDEFINED_OFFSET)
            {
                return CastSlot(ReadSlot(data, header->prefix_offset));
            }
            else
            {
                return SlotVal<R>{SlotValHeader<R>{0, 0}, nullptr};
            }
        }

        /**
         * TODO add append and reading without result in header
         */
        Key Split(byte *left_data, byte *right_data, page_id new_pid, Key key, R value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            UpdateHeapSize(right_data, 0);

            auto *left_header = CastHeader(left_data);

            auto *right_header = CastHeader(right_data);

            Slot *left_slots = OffsetHeader(left_data);

            Slot *right_slots = OffsetHeader(right_data);

            u32 count = left_header->count;
            DB7_ASSERT(count != 0, "nothing to copy");

            // copy half elements up
            u32 split = FindSplitPoint(left_data, left_slots, count);
            DB7_ASSERT(split > 0 && split < count, "split must leave tuples on both sides");

            // calc prefixes
            auto old_prefix = GetPrefixSlot(left_data); // CastSlot(ReadSlot(left_data, left_header->prefix_offset));
            // HACK
            Key old_prefix_copy{0, nullptr};
            if (old_prefix.hdr.len > 0)
            {
                byte *buf = new byte[old_prefix.hdr.len];
                std::memcpy(buf, old_prefix.data, old_prefix.hdr.len);

                old_prefix_copy = Key{
                    old_prefix.hdr.len,
                    buf};
            }

            // get fence keys
            auto sl1 = CastSlot(ReadSlot(left_data, left_slots[0]));
            auto sl2 = CastSlot(ReadSlot(left_data, left_slots[split]));
            auto sl3 = GetMaxValSlot(left_data);

            u16 prefix_len_l = left_header->llink != UNDEFINED ? CommonPrefixLen(sl1.data, sl2.data, sl1.hdr.len, sl2.hdr.len) : 0;
            u16 prefix_len_r = left_header->rlink != UNDEFINED ? CommonPrefixLen(sl2.data, sl3.data, sl2.hdr.len, sl3.hdr.len) : 0;

            u16 left_prefix_len = old_prefix.hdr.len + prefix_len_l;
            u16 right_prefix_len = old_prefix.hdr.len + prefix_len_r;

            // copy range of upper slots

            for (u32 i = split; i < count; i++)
            {
                SlotVal val = CastSlot(ReadSlot(left_data, left_slots[i]));
                u32 off = AppendHeap(right_data, OffsetCommonPrefix(val.data, val.hdr.len, prefix_len_r), val.hdr.result);
                right_slots[i - split] = Slot{off};
            }

            // append right prefixes
            u32 right_prefix = UNDEFINED_OFFSET;
            if (left_header->rlink != UNDEFINED)
            {
                right_prefix = ReserveSlot(right_data, right_prefix_len);
                SlotValHeader<R> *hdr = CastSlotHeader(right_data + right_prefix);
                hdr->result = value;
                hdr->len = right_prefix_len;
                std::memcpy(right_data + right_prefix + sizeof(SlotValHeader<R>), old_prefix.data, old_prefix.hdr.len);
                std::memcpy(right_data + right_prefix + sizeof(SlotValHeader<R>) + old_prefix.hdr.len, sl3.data, prefix_len_r);
            }

            // append max val from left node to right
            u64 right_max = UNDEFINED;
            if (left_header->max_val != UNDEFINED)
            {
                SlotVal val = CastSlot(ReadSlot(left_data, left_header->max_val));
                right_max = (u64)AppendHeap(right_data, Key{static_cast<u16>(val.hdr.len), val.data}, val.hdr.result);
            }

            // chose sentinel from rightmost page

            Key tmp = ReadKey(right_data, OffsetHeader(right_data)[0]);
            auto prefix = right_prefix != UNDEFINED_OFFSET
                              ? CastSlot(ReadSlot(right_data, right_prefix))
                              : SlotVal<R>{{0, 0}, 0};
            u16 len = tmp.len + prefix.hdr.len;
            byte *sentinel_copy = new byte[len];
            std::memcpy(sentinel_copy, prefix.data, prefix.hdr.len);
            std::memcpy(sentinel_copy + prefix.hdr.len, tmp.data, tmp.len);
            Key sentinel = Key{len, sentinel_copy};

            DB7_ASSERT(sentinel.len >= 0, "invalid len");

            // do compaction on left side
            CompactHeap(left_data, left_slots, split, prefix_len_l);

            // append left max val
            auto temp = Key{static_cast<u16>(sentinel.len), sentinel.data};
            u64 left_max = (u64)AppendHeap(left_data, temp, UNDEFINED);

            // append prefixes
            u32 left_prefix = UNDEFINED_OFFSET;
            if (left_header->llink != UNDEFINED)
            {
                left_prefix = ReserveSlot(left_data, left_prefix_len);
                SlotValHeader<R> *hdr = CastSlotHeader(left_data + left_prefix);
                hdr->result = value;
                hdr->len = left_prefix_len;
                std::memcpy(left_data + left_prefix + sizeof(SlotValHeader<R>), old_prefix_copy.data, old_prefix_copy.len);
                std::memcpy(left_data + left_prefix + sizeof(SlotValHeader<R>) + old_prefix_copy.len, sentinel.data + old_prefix_copy.len, prefix_len_l);
            }

            u32 left_header_count = split;
            u32 right_header_count = left_header->count - split;

            if (Cmp(sentinel, key) > 0)
            {
                auto new_key = Key{static_cast<u16>(key.len - left_prefix_len), key.data + left_prefix_len};
                InsertInternal(left_data, left_header_count++, new_key, value);
            }
            else
            {
                auto new_key = Key{static_cast<u16>(key.len - right_prefix_len), key.data + right_prefix_len};
                InsertInternal(right_data, right_header_count++, new_key, value);
            }

            // TODO write rest of headers

            WriteHeader(right_header, new_pid, left_header->rlink, left_header->pid, right_header_count, left_header->level, right_max, right_prefix, right_prefix_len);

            WriteHeader(left_header, left_header->pid, new_pid, left_header->llink, left_header_count, left_header->level, left_max, left_prefix, left_prefix_len);

            auto deep_cpy = DeepCopyEncoded(sentinel);

            delete[] sentinel.data;

            return deep_cpy;

            // u32 mid = CopyUpperHalf(left_data, right_data, left_header->count);

            // Slot *left_slots = OffsetHeader(left_data);
            // u64 right_max = UNDEFINED;
            // if (left_header->max_val != UNDEFINED)
            // {
            //     SlotVal val = CastSlot(ReadSlot(left_data, left_header->max_val));
            //     right_max = (u64)AppendHeap(right_data, Key{val.hdr.len, val.data}, val.hdr.result);
            // }

            // Key sentinel = DeepCopy(ReadKey(left_data, OffsetHeader(left_data)[mid]));

            // Key sentinel_copy = DeepCopyEncoded(sentinel);

            // CompactHeap(left_data, left_slots, mid);

            // u64 left_max = (u64)AppendHeap(left_data, sentinel, UNDEFINED);

            // u32 left_header_count = mid;
            // u32 right_header_count = left_header->count - mid;

            // if (Cmp(sentinel, key) > 0)
            // {
            //     InsertInternal(left_data, left_header_count++, key, value);
            // }
            // else
            // {
            //     InsertInternal(right_data, right_header_count++, key, value);
            // }

            // WriteHeader(right_header, new_pid, left_header->rlink, left_header->pid, right_header_count, left_header->level, right_max);

            // WriteHeader(left_header, left_header->pid, new_pid, left_header->llink, left_header_count, left_header->level, left_max);

            // delete[] sentinel.data;

            // return sentinel_copy;
        }

        void InitHeader(byte *data, u32 count, u8 level, page_id pid)
        {
            WriteHeader(data, pid, UNDEFINED, UNDEFINED, count, level, UNDEFINED, UNDEFINED_OFFSET, 0);
        }

        u64 GetRLink(byte *data)
        {
            return CastHeader(data)->rlink;
        }
    };

}