
#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "access/index/layouts/varlen/varlen_layout_models.hpp"

#include <limits>
#include <algorithm>
#include <vector>

namespace db7::access
{
    template <typename R>
    struct SlotValHeaderInter
    {
        R result;
        u16 len;
    };

    template <typename R>
    struct SlotValInter
    {
        SlotValHeaderInter<R> hdr;
        byte *data;

        SlotValInter(byte *page_data, u16 heap_offset)
            : hdr(*reinterpret_cast<SlotValHeaderInter<R> *>(page_data + heap_offset)),
              data(page_data + heap_offset + sizeof(SlotValHeaderInter<R>)) {}

        // NOTE: this one threats value(result) as 0 so it navigates to the leftmost value
        SlotValInter(Key key) : hdr({0, key.enc_len}), data(key.data) {}

        SlotValInter(Key key, R value) : hdr({value, key.enc_len}), data(key.data) {}
    };

    template <typename ValTyp>
    class BtreeVarlenLayoutIntermediate
    {
    private:
        static constexpr auto header_size_ = sizeof(VarlenHeader);

        /** Same as in leaf shoul be templated */
        u16 CalculateHeapOffset(u16 prev_heap_offset, u16 tuple_len)
        {
            u16 heap_offset = prev_heap_offset - tuple_len - sizeof(SlotValHeaderInter<ValTyp>);
            return shared::AlignDown(heap_offset, alignof(SlotValHeaderInter<ValTyp>));
        }

        u16 AppendHeap(byte *data, Key key, ValTyp value)
        {
            u16 &prev_heap_offset = VarlenHeader::CastHeader(data)->heap_offset;
            /* Aligns entry for SlotValHeaderLeaf metadata */
            u16 new_heap_offset = CalculateHeapOffset(prev_heap_offset, key.enc_len);
            /* Fill header */
            auto *hdr = reinterpret_cast<SlotValHeaderInter<ValTyp> *>(data + new_heap_offset);
            *hdr = {value, key.enc_len};
            /* Fill value */
            std::memcpy(data + new_heap_offset + sizeof(SlotValHeaderInter<ValTyp>), key.data, key.enc_len);
            /* Update heap offset to point to new value */
            prev_heap_offset = new_heap_offset;

            return new_heap_offset;
        }

        u16 MoveHeap(byte *data, u16 old_offset, u16 len)
        {
            u16 &prev_heap_offset = VarlenHeader::CastHeader(data)->heap_offset;
            /* Aligns entry for SlotValHeaderLeaf metadata */
            u16 new_heap_offset = CalculateHeapOffset(prev_heap_offset, len);
            /* Fill value */
            std::memmove(data + new_heap_offset, data + old_offset, sizeof(SlotValHeaderInter<ValTyp>) + len);
            /* Update heap offset to point to new value */
            prev_heap_offset = new_heap_offset;

            DB7_ASSERT(new_heap_offset >= old_offset, "compaction must not move tuples down");

            return new_heap_offset;
        }

        u16 *CastSlots(byte *data)
        {
            return reinterpret_cast<u16 *>(data + header_size_);
        }

        /**
         * negative → slot < main
         * zero → slot == main
         * positive → slot > main
         */
        inline int CmpKeys(SlotValInter<ValTyp> slot_val, SlotValInter<ValTyp> main_val)
        {
            /* Find min value between 2 payloads */
            u32 min_len = std::min(main_val.hdr.len, slot_val.hdr.len);
            /* Compare their values */
            int cmp = std::memcmp(slot_val.data, main_val.data, min_len);
            if (cmp != 0)
                return cmp;
            /* If values are the same compare lens */
            return (main_val.hdr.len < slot_val.hdr.len) - (main_val.hdr.len > slot_val.hdr.len);
        } // TODO fix index do i need len check if my encoder adds a 0 at the end

        /**
         * As CmpKeys, but breaks ties on the heap tuple identifier
         * so that (key, tid) is a total order.
         */
        inline int Cmp(SlotValInter<ValTyp> slot_val, SlotValInter<ValTyp> main_val)
        {
            int cmp = CmpKeys(slot_val, main_val);
            if (cmp != 0)
                return cmp;
            /* If lens are the same compare with heap tuple identifier (page,idx in page) */
            return (slot_val.hdr.result > main_val.hdr.result) - (slot_val.hdr.result < main_val.hdr.result);
        }

        int FindInsertPosition(byte *data, u16 *slots, SlotValInter<ValTyp> main_val, u16 count)
        {
            int hi = count, lo = 0;
            while (lo < hi) // TODO fix index <= or <
            {
                int mid = lo + (hi - lo) / 2;
                auto slot_val = SlotValInter<ValTyp>(data, slots[mid]);
                int res = Cmp(slot_val, main_val);
                DB7_ASSERT(res != 0, "Somehow there are identical entries in the tree. That means same tuple was inserted twice");
                if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        void ShiftRightInsert(u16 *slots, int idx, u16 count, u16 heap_offset)
        {
            std::memmove(slots + idx + 1, slots + idx, (count - idx) * sizeof(u16));
            slots[idx] = heap_offset;
        }

        void InsertSlot(byte *data, u16 heap_offset)
        {
            u16 *slots = CastSlots(data);

            auto main_val = SlotValInter<ValTyp>(data, heap_offset);

            u16 &count = VarlenHeader::CastHeader(data)->count;

            int idx = FindInsertPosition(data, slots, main_val, count);

            ShiftRightInsert(slots, idx, count, heap_offset);

            count++;
        }

        u16 CopyHalfRight(byte *__restrict left_data, byte *__restrict right_data, u16 split)
        {
            auto *left_header = VarlenHeader::CastHeader(left_data);
            u16 *left_slots = CastSlots(left_data);
            u16 *right_slots = CastSlots(right_data);

            u16 right_max = UNDEFINED_OFFSET;
            if (left_header->max_val != UNDEFINED_OFFSET)
            {
                auto slot = SlotValInter<ValTyp>(left_data, left_header->max_val);
                right_max = AppendHeap(right_data, {slot.hdr.len, slot.hdr.len, slot.data}, slot.hdr.result);
            }

            for (int i = split; i < left_header->count; i++)
            {
                auto slot = SlotValInter<ValTyp>(left_data, left_slots[i]);
                u16 offset = AppendHeap(right_data, {slot.hdr.len, slot.hdr.len, slot.data}, slot.hdr.result);
                right_slots[i - split] = offset;
            }

            return right_max;
        }

        /*
         * Repack the first `count` tuples against the top of the page and
         * rewrite their slot offsets. Does not preserve the high fence: the
         * caller re-appends it afterwards if it still wants one.
         */
        void CompactHeap(byte *data, u32 count)
        {
            DB7_ASSERT(VarlenHeader::CastHeader(data)->heap_offset == DB7_PAGE_SIZE, "heap not set correctly");
            u16 *slots = CastSlots(data);

            std::vector<u16> indexes;
            indexes.reserve(count);
            for (u32 i = 0; i < count; i++)
                indexes.emplace_back(i);

            std::sort(indexes.begin(), indexes.end(), [&](u32 a, u32 b)
                      { return slots[a] > slots[b]; });

            for (u32 i = 0; i < count; i++)
            {
                u16 idx = indexes[i];
                auto *hdr = reinterpret_cast<SlotValHeaderInter<ValTyp> *>(data + slots[idx]);
                slots[idx] = MoveHeap(data, slots[idx], hdr->len);
            }
        }

        u16 FindSplitPoint(byte *data, u16 *slots, u16 count)
        {
            DB7_ASSERT(count >= 2, "cannot split fewer than two tuples");

            u32 live = 0;
            for (int i = 0; i < count; i++)
                live += reinterpret_cast<SlotValHeaderInter<ValTyp> *>(data + slots[i])->len;

            u32 target = live / 2;
            u32 accumulated = 0;

            for (int i = 0; i < count; i++)
            {
                accumulated += reinterpret_cast<SlotValHeaderInter<ValTyp> *>(data + slots[i])->len;
                if (accumulated >= target)
                    return std::min(u16(i) + 1, count - 1);
            }

            return count - 1;
        }

    public:
        static constexpr page_id UNDEFINED_PAGE = std::numeric_limits<page_id>::max();
        static constexpr u32 UNDEFINED_OFFSET = std::numeric_limits<u16>::max();

        BtreeVarlenLayoutIntermediate() = default;

        ValTyp Get(byte *data, const u32 count, const Key key, const ValTyp value = 0)
        {
            u16 *slots = CastSlots(data);
            const auto main_val = SlotValInter<ValTyp>(key, value);
            u16 idx = FindInsertPosition(data, slots, main_val, count);
            DB7_ASSERT(idx > 0, "no covering child — leftmost separator must be the empty key");
            return reinterpret_cast<SlotValHeaderInter<ValTyp> *>(data + slots[idx - 1])->result;
        }

        void Insert(byte *data, Key key, ValTyp value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");
            DB7_ASSERT(key.len < DB7_PAGE_SIZE / 10, "should be checked in the binder");
            DB7_ASSERT(value != 0, "Can not insert 0 which is invalid page");

            u16 tuple_heap_offset = AppendHeap(data, key, value);
            InsertSlot(data, tuple_heap_offset);
        }

        // TODO fix index same as in leaf
        bool HasSpace(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");

            const auto *header = VarlenHeader::CastHeader(data);
            const u16 count = header->count;
            const u16 slot_size = (count + 1) * sizeof(u16);
            const u16 prev_heap_offset = header->heap_offset;
            const u16 new_heap_offset = CalculateHeapOffset(prev_heap_offset, key.enc_len);
            return new_heap_offset >= slot_size + header_size_;
        }

        bool HasSplit(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");

            const auto *header = VarlenHeader::CastHeader(data);
            if (header->max_val == UNDEFINED_OFFSET)
            {
                return false;
            }
            const auto max_val = SlotValInter<ValTyp>(data, header->max_val);
            const auto main_val = SlotValInter<ValTyp>(key);
            int cmp = Cmp(max_val, main_val);
            DB7_ASSERT(cmp != 0, "Can not have value to be 0 when inserting the tree");
            return cmp < 0;
        }

        /**
         * TODO might be better to use thread local buffer for this case
         * to avoid copying objects
         */
        Key Split(byte *__restrict left_data, byte *__restrict right_data, ValTyp new_pid, Key key, ValTyp value)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.enc_len != 0, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            auto *left_header = VarlenHeader::CastHeader(left_data);
            auto *right_header = VarlenHeader::CastHeader(right_data);
            u16 *left_slots = CastSlots(left_data);
            u16 *right_slots = CastSlots(right_data);

            DB7_ASSERT(right_header->heap_offset == DB7_PAGE_SIZE, "Heap must not be filled");

            // find split point
            u16 split = FindSplitPoint(left_data, left_slots, left_header->count);

            // move count - split elems right
            u16 right_max = CopyHalfRight(left_data, right_data, split);

            // compact left page
            left_header->heap_offset = DB7_PAGE_SIZE;
            CompactHeap(left_data, split);

            // insert max right
            auto sentinel = SlotValInter<ValTyp>(right_data, right_slots[0]);
            u16 left_max = AppendHeap(left_data, {sentinel.hdr.len, sentinel.hdr.len, sentinel.data}, sentinel.hdr.result);

            // update headers
            right_header->rlink = left_header->rlink;
            right_header->count = left_header->count - split;
            right_header->max_val = right_max;

            left_header->rlink = new_pid;
            left_header->count = split;
            left_header->max_val = left_max;

            // insert key
            auto main_val = SlotValInter<ValTyp>(key, value);
            int cmp = Cmp(sentinel, main_val);
            if (cmp < 0)
            {
                // insert right
                DB7_ASSERT(HasSpace(right_data, key), "new key does not fit in right half after split");
                u16 tuple_heap_offset = AppendHeap(right_data, key, value);
                InsertSlot(right_data, tuple_heap_offset);
            }
            else
            {
                // insert left
                DB7_ASSERT(HasSpace(left_data, key), "new key does not fit in left half after split");
                u16 tuple_heap_offset = AppendHeap(left_data, key, value);
                InsertSlot(left_data, tuple_heap_offset);
            }

            // copy/send sentinel up
            byte *buf = new byte[sentinel.hdr.len]; // TODO fix index
            std::memcpy(buf, sentinel.data, sentinel.hdr.len);
            return {sentinel.hdr.len, sentinel.hdr.len, buf};
        }

        void CreateRoot(byte *data, Key key, ValTyp pid, ValTyp new_pid)
        {
            u16 *slots = CastSlots(data);
            slots[0] = AppendHeap(data, {}, pid); // Empty key inserted
            slots[1] = AppendHeap(data, key, new_pid);
            VarlenHeader::CastHeader(data)->count = 2;
        }

        void InitHeader(byte *data, u32 count, u8 level, page_id pid)
        {
            VarlenHeader::WriteHeader(data, pid, UNDEFINED_PAGE, count, UNDEFINED_OFFSET, level, DB7_PAGE_SIZE);
        }

        u32 GetRLink(byte *data)
        {
            return VarlenHeader::CastHeader(data)->rlink;
        }
    };
}