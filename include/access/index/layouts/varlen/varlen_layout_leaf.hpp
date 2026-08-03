#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "access/index/layouts/varlen/varlen_layout_models.hpp"
#include "access/key_encoder.hpp"

#include <limits>
#include <algorithm>
#include <span>
#include <exception>

namespace db7::access
{
#define DB7_MAX_SLOTS_PER_PAGE ((DB7_PAGE_SIZE - sizeof(VarlenHeader<ValTyp>)) / sizeof(u32))

    template <typename R>
    struct SlotValHeaderLeaf
    {
        R result;
        u16 len;
        u16 enc_len;
    };

    template <typename R>
    struct SlotValLeaf
    {
        SlotValHeaderLeaf<R> hdr;
        byte *data;
    };

    template <typename ValTyp>
    class BtreeVarlenLayoutLeaf
    {
    private:
        u64 key_offset_;

        template <typename Typ>
        void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
        {
            std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
            data[idx] = value;
        }

        template <typename Typ>
        void ShiftLeftDelete(Typ *data, u32 count, u32 idx)
        {
            std::memmove(data + idx, data + idx + 1, (count - idx - 1) * sizeof(Typ));
        }

        int Cmp(byte *slot, Key key)
        {
            SlotValLeaf<ValTyp> val = CastSlot(slot);
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

        SlotValHeaderLeaf<ValTyp> *CastSlotHeader(void *slot)
        {
            return reinterpret_cast<SlotValHeaderLeaf<ValTyp> *>(slot);
        }

        SlotValLeaf<ValTyp> CastSlot(void *slot)
        {
            auto hdr = *CastSlotHeader(slot);
            return SlotValLeaf<ValTyp>{hdr, static_cast<byte *>(slot) + sizeof(hdr)};
        }

        Slot *OffsetHeader(byte *data)
        {
            return reinterpret_cast<Slot *>(data + key_offset_);
        }

        u32 CalcWorstCaseSize(Key key)
        {
            return key.len + sizeof(SlotValHeaderLeaf<ValTyp>) + alignof(SlotValHeaderLeaf<ValTyp>);
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
            auto *hdr = VarlenHeader<ValTyp>::CastHeader(data);
            hdr->heap_size = val;
        }

        /* includes slot header */
        u32 ReserveSlot(byte *data, u32 len)
        {
            u32 heap_size = VarlenHeader<ValTyp>::CastHeader(data)->heap_size;
            DB7_ASSERT(heap_size < DB7_PAGE_SIZE, "heap overflow");
            u32 off = shared::AlignDown(DB7_PAGE_SIZE - heap_size - len - sizeof(SlotValHeaderLeaf<ValTyp>), alignof(SlotValHeaderLeaf<ValTyp>));
            UpdateHeapSize(data, DB7_PAGE_SIZE - off);
            return off;
        }

        /* Insert to heap */
        u32 AppendHeap(byte *data, Key key, ValTyp value)
        {
            u32 off = ReserveSlot(data, key.len);

            DB7_ASSERT(off > 0 && off < DB7_PAGE_SIZE, "heap overflow");

            SlotValHeaderLeaf<ValTyp> *hdr = CastSlotHeader(data + off);
            hdr->result = value;
            hdr->enc_len = key.enc_len;
            hdr->len = key.len;
            std::memcpy(data + off + sizeof(SlotValHeaderLeaf<ValTyp>), key.data, key.len);

            return off;
        }

        u32 EntrySize(const SlotValHeaderLeaf<ValTyp> *hdr)
        {
            return shared::AlignUp(
                (u32)(sizeof(SlotValHeaderLeaf<ValTyp>) + hdr->len),
                (u32)alignof(SlotValHeaderLeaf<ValTyp>));
        }

        /*
         * Split at the point that puts roughly half the *live* tuple bytes on
         * each side. heap_size is not usable as the target here: it also covers
         * the high fence and any dead space, so halving it can walk off the end
         * of the slot array.
         */
        u32 FindSplitPoint(byte *data, Slot *slots, u32 count)
        {
            DB7_ASSERT(count >= 2, "cannot split fewer than two tuples");

            u32 live = 0;
            for (u32 i = 0; i < count; i++)
                live += EntrySize(CastSlotHeader(ReadSlot(data, slots[i])));

            u32 target = live / 2;
            u32 accumulated = 0;

            for (u32 i = 0; i < count; i++)
            {
                accumulated += EntrySize(CastSlotHeader(ReadSlot(data, slots[i])));
                if (accumulated >= target)
                    return std::min(i + 1, count - 1);
            }

            return count - 1;
        }

        /*
         * Repack the first `count` tuples against the top of the page and
         * rewrite their slot offsets. Does not preserve the high fence: the
         * caller re-appends it afterwards if it still wants one.
         */
        void PackTuples(byte *data, Slot *slots, u32 count)
        {
            // sort slot indices by offset descending (highest first = end of page)
            u32 indices[DB7_MAX_SLOTS_PER_PAGE]; // TODO move to heap
            for (u32 i = 0; i < count; i++)
                indices[i] = i;

            std::sort(indices, indices + count, [&](u32 a, u32 b)
                      { return slots[a].offset > slots[b].offset; });

            u32 write_pos = 0;
            for (u32 i = 0; i < count; i++)
            {
                u32 idx = indices[i];
                SlotValLeaf<ValTyp> val = CastSlot(ReadSlot(data, slots[idx]));

                u32 entry_size = sizeof(SlotValHeaderLeaf<ValTyp>) + val.hdr.len;
                u32 off = shared::AlignDown(DB7_PAGE_SIZE - write_pos - entry_size,
                                            alignof(SlotValHeaderLeaf<ValTyp>));

                std::memmove(data + off + sizeof(SlotValHeaderLeaf<ValTyp>),
                             val.data, val.hdr.len);
                auto *new_hdr = CastSlotHeader(data + off);
                new_hdr->len = val.hdr.len;
                new_hdr->enc_len = val.hdr.enc_len;
                new_hdr->result = val.hdr.result;

                slots[idx].offset = off;
                write_pos = DB7_PAGE_SIZE - off;
            }

            UpdateHeapSize(data, write_pos);
            VarlenHeader<ValTyp>::CastHeader(data)->dead_space = 0;
        }

        /* Reclaim dead space in place, keeping the high fence. */
        void CompactHeap(byte *data, Slot *slots, u32 count)
        {
            auto *header = VarlenHeader<ValTyp>::CastHeader(data);

            /* snapshot the fence BEFORE moving anything */
            bool has_max = ReadMaxVal(header) != UNDEFINED_OFFSET;
            SlotValHeaderLeaf<ValTyp> max_hdr{};
            std::unique_ptr<byte[]> max_data;
            if (has_max)
            {
                SlotValLeaf<ValTyp> mv = CastSlot(ReadSlot(data, ReadMaxVal(header)));
                max_hdr = mv.hdr;
                max_data = std::make_unique<byte[]>(mv.hdr.len);
                std::memcpy(max_data.get(), mv.data, mv.hdr.len);
            }

            PackTuples(data, slots, count);

            if (has_max)
                header->max_val = AppendHeap(data,
                                             Key{max_hdr.len, max_hdr.enc_len, max_data.get()},
                                             max_hdr.result);
        }

        Key ReadKey(byte *data, Slot slot)
        {
            byte *ptr = ReadSlot(data, slot);
            SlotValHeaderLeaf<ValTyp> *hdr = CastSlotHeader(ptr);
            return Key{hdr->len, hdr->enc_len, ptr + sizeof(SlotValHeaderLeaf<ValTyp>)};
        }

        Key ReadKey(byte *data, u32 offset)
        {
            return ReadKey(data, Slot{offset});
        }

        Key KeyOf(SlotValLeaf<ValTyp> val)
        {
            return Key{val.hdr.len, val.hdr.enc_len, val.data};
        }

        Key DeepCopyEncoded(Key key)
        {
            u32 BASE_OVERHEAD = key.enc_len * 10; // TODO store this somewhere, in the tree for example based on schema
            byte *sentinel_copy = new byte[key.enc_len + BASE_OVERHEAD];
            return MakeEncodedKey(key.enc_len, key.data);
        }

        ResultObj<void> InsertInternal(byte *data, u32 count, Key key, ValTyp value)
        {
            /* Insert slot */
            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            if (found)
            {
                return ResultObj<void>::Fail("Key already exists\0");
            }

            Slot *slots = OffsetHeader(data);

            /* Insert to heap */
            u32 off = AppendHeap(data, key, value);

            Slot slot = Slot{off};
            ShiftRightInsert(slots, count, idx, slot); // TODO this should increment header count

            return ResultObj<void>::Ok();
        }

        u32 ReadMaxVal(VarlenHeader<ValTyp> *header)
        {
            return static_cast<u32>(header->max_val);
        }

        ResultObj<void> DeleteInternal(byte *data, Key key)
        {
            auto *header = VarlenHeader<ValTyp>::CastHeader(data);
            Slot *slots = OffsetHeader(data);
            u32 count = header->count;

            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            if (!found)
            {
                return ResultObj<void>::Fail("Key not found\0");
            }

            SlotValHeaderLeaf<ValTyp> *hdr = CastSlotHeader(ReadSlot(data, slots[idx]));
            header->dead_space += EntrySize(hdr);

            ShiftLeftDelete(slots, count, idx);

            count--;

            header->count = count;

            if (header->dead_space > DB7_PAGE_SIZE / 4)
                CompactHeap(data, slots, count);

            return ResultObj<void>::Ok();
        }

    public:
        static constexpr ValTyp UNDEFINED = std::numeric_limits<ValTyp>::max();
        static constexpr u32 UNDEFINED_OFFSET = std::numeric_limits<u32>::max();

        BtreeVarlenLayoutLeaf() : key_offset_(sizeof(VarlenHeader<ValTyp>)) {}

        ResultObj<ValTyp> Get(byte *data, const u32 count, const Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            Slot *slots = OffsetHeader(data);

            bool found = false;
            u32 idx = GetIdx(data, count, key, found);
            if (found)
            {
                SlotValLeaf<ValTyp> val = CastSlot(ReadSlot(data, slots[idx]));
                return {val.hdr.result, true};
            }

            return {"Key not found\0", false};
        }

        ResultObj<void> Insert(byte *data, Key key, ValTyp value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            u32 count = VarlenHeader<ValTyp>::CastHeader(data)->count;
            auto result = InsertInternal(data, count, key, value);
            if (result.success)
                VarlenHeader<ValTyp>::CastHeader(data)->count++;
            return result;
        }

        bool HasSpace(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            auto hdr = VarlenHeader<ValTyp>::CastHeader(data); // TODO fix this, this can all fit into taken_space
            return key_offset_ + hdr->count * sizeof(Slot) + hdr->heap_size + CalcWorstCaseSize(key) < DB7_PAGE_SIZE;
        }

        bool HasSplit(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            auto *header = VarlenHeader<ValTyp>::CastHeader(data);
            if (ReadMaxVal(header) == UNDEFINED_OFFSET)
                return false; // rightmost page, no high key
            auto *slot = ReadSlot((byte *)header, ReadMaxVal(header));
            // DB7_ASSERT((Cmp(slot, key) <= 0) == false, "node has split(this is for single thread only)"); // TODO comment
            return Cmp(slot, key) <= 0;
        }

        /**
         * TODO might be better to use thread local buffer for this case
         * to avoid copying objects
         */
        ResultObj<Key> Split(byte *__restrict left_data, byte *__restrict right_data, ValTyp new_pid, Key key, ValTyp value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            auto *left_header = VarlenHeader<ValTyp>::CastHeader(left_data);
            auto *right_header = VarlenHeader<ValTyp>::CastHeader(right_data);

            Slot *left_slots = OffsetHeader(left_data);
            Slot *right_slots = OffsetHeader(right_data);

            UpdateHeapSize(right_data, 0);
            right_header->dead_space = 0;

            u32 count = left_header->count;
            DB7_ASSERT(count != 0, "nothing to copy");

            u32 split = FindSplitPoint(left_data, left_slots, count);
            DB7_ASSERT(split > 0 && split < count, "split must leave tuples on both sides");

            /*
             * Separator is the first key that moves to the right node. Copy it
             * out: the left heap gets repacked underneath us further down.
             */
            Key tmp_sentinel = ReadKey(left_data, left_slots[split]);
            auto sentinel_buf = std::make_unique<byte[]>(tmp_sentinel.len);
            std::memcpy(sentinel_buf.get(), tmp_sentinel.data, tmp_sentinel.len);
            Key sentinel = Key{tmp_sentinel.len, tmp_sentinel.enc_len, sentinel_buf.get()};

            /* move the upper half of the slots to the right node */
            for (u32 i = split; i < count; i++)
            {
                SlotValLeaf<ValTyp> val = CastSlot(ReadSlot(left_data, left_slots[i]));
                u32 off = AppendHeap(right_data, KeyOf(val), val.hdr.result);
                right_slots[i - split] = Slot{off};
            }

            /* left's old high fence becomes right's high fence */
            u64 right_max = UNDEFINED_OFFSET;
            if (ReadMaxVal(left_header) != UNDEFINED_OFFSET)
            {
                SlotValLeaf<ValTyp> val = CastSlot(ReadSlot(left_data, ReadMaxVal(left_header)));
                right_max = (u64)AppendHeap(right_data, KeyOf(val), val.hdr.result);
            }

            /* reclaim the space vacated on the left; this drops the old fence */
            PackTuples(left_data, left_slots, split);

            /* the separator becomes left's new high fence */
            u64 left_max = (u64)AppendHeap(left_data, sentinel, UNDEFINED);

            /* finally insert main key */
            u32 left_header_count = split;
            u32 right_header_count = count - split;
            ResultObj<void> result;
            if (Cmp(sentinel, key) > 0)
                result = InsertInternal(left_data, left_header_count++, key, value);
            else
                result = InsertInternal(right_data, right_header_count++, key, value);

            if (!result.success)
            {
                return {result.message, false};
            }

            /* update headers */
            right_header->WriteHeader(new_pid, left_header->rlink, left_header->pid, right_header_count, left_header->level, right_max, UNDEFINED_OFFSET, 0);

            left_header->WriteHeader(left_header->pid, new_pid, left_header->llink, left_header_count, left_header->level, left_max, UNDEFINED_OFFSET, 0);

            /* encode a string (lib does the allocation) */
            // TODO can reuse existing sentinel buffer in future
            return {Key{sentinel.len, sentinel.enc_len, sentinel_buf.release()}, true};
        }

        void InitHeader(byte *data, u32 count, u8 level, ValTyp pid)
        {
            VarlenHeader<ValTyp>::WriteHeader(data, pid, UNDEFINED, UNDEFINED, count, level, UNDEFINED_OFFSET, UNDEFINED_OFFSET, 0);
        }

        u64 GetRLink(byte *data)
        {
            return VarlenHeader<ValTyp>::CastHeader(data)->rlink;
        }

        ResultObj<void> Delete(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            return DeleteInternal(data, key);
        }
    };
}