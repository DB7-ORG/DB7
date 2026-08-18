#pragma once

#include "common.hpp"
#include "shared/align_util.hpp"
#include "access/index/layouts/varlen/varlen_layout_models.hpp"
#include "access/key_encoder.hpp"
#include "shared/byte_utils.hpp"
#include "shared/models/vector_result.hpp"
#include "shared/models/tuple_id.hpp"
#include "transaction/transaction_context.hpp"

#include <limits>
#include <algorithm>
#include <span>
#include <exception>
#include <vector>

namespace db7::access
{
    template <typename R>
    struct SlotValLeaf
    {
        u16 len;
        byte *data;

        SlotValLeaf(byte *page_data, u16 heap_offset) : len(*reinterpret_cast<u16 *>(page_data + heap_offset)),
                                                        data(page_data + heap_offset + sizeof(u16)) {}

        SlotValLeaf(Key key) : len(key.len), data(key.data) {}

        R Result()
        {
            R v;
            std::memcpy(&v, data + len - sizeof(R), sizeof(v));
            return shared::ByteUtil::ByteSwapIfLittleEndian(v);
        }
    };

    template <typename ValTyp>
    class BtreeVarlenLayoutLeaf : public BaseLayout
    {
    private:
        static constexpr auto header_size_ = sizeof(VarlenHeader);

        u16 CalculateHeapOffset(u16 prev_heap_offset, u16 tuple_len)
        {
            u16 heap_offset = prev_heap_offset - tuple_len - sizeof(u16);
            return shared::AlignDown(heap_offset, alignof(u16));
        }

        u16 AppendHeap(byte *data, Key key)
        {
            u16 &prev_heap_offset = VarlenHeader::CastHeader(data)->heap_offset;
            /* Aligns entry for SlotValHeaderLeaf metadata */
            u16 new_heap_offset = CalculateHeapOffset(prev_heap_offset, key.len);
            /* Fill header */
            auto *hdr = reinterpret_cast<u16 *>(data + new_heap_offset);
            *hdr = key.len;
            /* Fill value */
            std::memcpy(data + new_heap_offset + sizeof(u16), key.data, key.len);
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
            std::memmove(data + new_heap_offset, data + old_offset, sizeof(u16) + len);
            /* Update heap offset to point to new value */
            prev_heap_offset = new_heap_offset;

            DB7_ASSERT(new_heap_offset >= old_offset, "compaction must not move tuples down");

            return new_heap_offset;
        }

        /**
         * negative → slot < main
         * zero → slot == main
         * positive → slot > main
         */
        inline int CmpPrefix(SlotValLeaf<ValTyp> slot_val, SlotValLeaf<ValTyp> main_val)
        {
            /* Find min value between 2 payloads */
            // DB7_ASSERT(main_val.len >= sizeof(ValTyp) && slot_val.len >= sizeof(ValTyp), "key missing tid");
            u32 min_len = std::min(main_val.len, slot_val.len) - sizeof(ValTyp);
            if (min_len >= DB7_MAX_ROW_SIZE)
                return 1; // returns dummy value since there is a concurrent reader/writer
            /* Compare their values */
            int cmp = std::memcmp(slot_val.data, main_val.data, min_len);
            if (cmp != 0)
                return cmp;
            /* If values are the same compare lens */
            return (main_val.len < slot_val.len) - (main_val.len > slot_val.len);
        }

        /**
         * As CmpPrefix, but breaks ties on the heap tuple identifier
         * so that (key, tid) is a total order.
         */
        inline int CmpFull(SlotValLeaf<ValTyp> slot_val, SlotValLeaf<ValTyp> main_val)
        {
            /* Find min value between 2 payloads */
            u32 min_len = std::min(main_val.len, slot_val.len);
            /* Compare their values */
            int cmp = std::memcmp(slot_val.data, main_val.data, min_len);
            if (cmp != 0)
                return cmp;
            /* If values are the same compare lens */
            return (main_val.len < slot_val.len) - (main_val.len > slot_val.len);
        }

        int FindInsertPosition(byte *data, u16 *slots, SlotValLeaf<ValTyp> main_val, u16 count)
        {
            int hi = count, lo = 0;
            while (lo < hi)
            {
                int mid = lo + (hi - lo) / 2;
                auto slot_val = SlotValLeaf<ValTyp>(data, slots[mid]);
                int res = CmpFull(slot_val, main_val);
                // NOTE: this war removed since im doing optimistic reading which with an active
                // writer can produce any of the results but its fine since the thread will retry later
                // DB7_ASSERT_FMT(res != 0, "duplicate (key,tid): mid={} count={} len={} tid={}",
                //                mid, count, main_val.len, (unsigned long long)main_val.Result());
                if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        int FindStartPosition(byte *data, u16 *slots, SlotValLeaf<ValTyp> main_val, u16 count)
        {
            int hi = count, lo = 0;
            while (lo < hi)
            {
                int mid = lo + (hi - lo) / 2;
                auto slot_val = SlotValLeaf<ValTyp>(data, slots[mid]);
                int res = CmpPrefix(slot_val, main_val);
                // NOTE: this war removed since im doing optimistic reading which with an active
                // writer can produce any of the results but its fine since the thread will retry later
                // DB7_ASSERT_FMT(res != 0, "duplicate (key,tid): mid={} count={} len={} tid={}",
                //                mid, count, main_val.len, (unsigned long long)main_val.Result());
                if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        int FindDeletePosition(byte *data, u16 *slots, SlotValLeaf<ValTyp> main_val, u16 count)
        {
            int hi = count, lo = 0;
            while (lo < hi)
            {
                int mid = lo + (hi - lo) / 2;
                auto slot_val = SlotValLeaf<ValTyp>(data, slots[mid]);
                int res = CmpFull(slot_val, main_val);
                if (res == 0)
                    return mid;
                else if (res < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        void InsertSlot(byte *data, u16 heap_offset)
        {
            u16 *slots = CastSlots(data);

            auto main_val = SlotValLeaf<ValTyp>(data, heap_offset);

            u16 &count = VarlenHeader::CastHeader(data)->count;

            int idx = FindInsertPosition(data, slots, main_val, count);

            ShiftRightInsert(slots, idx, count, heap_offset);

            count++;
        }

        u16 FindSplitPoint(byte *data, u16 *slots, u16 count)
        {
            DB7_ASSERT(count >= 2, "cannot split fewer than two tuples");

            u32 live = 0;
            for (int i = 0; i < count; i++)
                live += *reinterpret_cast<u16 *>(data + slots[i]);

            u32 target = live / 2;
            u32 accumulated = 0;

            for (int i = 0; i < count; i++)
            {
                accumulated += *reinterpret_cast<u16 *>(data + slots[i]);
                if (accumulated >= target)
                    return std::min(u16(i) + 1, count - 1);
            }

            return count - 1;
        }

        u16 CopyHalfRight(byte *__restrict left_data, byte *__restrict right_data, u16 split)
        {
            auto *left_header = VarlenHeader::CastHeader(left_data);
            u16 *left_slots = CastSlots(left_data);
            u16 *right_slots = CastSlots(right_data);

            u16 right_max = UNDEFINED_OFFSET;
            if (left_header->max_val != UNDEFINED_OFFSET)
            {
                auto slot = SlotValLeaf<ValTyp>(left_data, left_header->max_val);
                right_max = AppendHeap(right_data, {slot.len, slot.data});
            }

            for (int i = split; i < left_header->count; i++)
            {
                auto slot = SlotValLeaf<ValTyp>(left_data, left_slots[i]);
                u16 offset = AppendHeap(right_data, {slot.len, slot.data});
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
                u16 len = *reinterpret_cast<u16 *>(data + slots[idx]);
                slots[idx] = MoveHeap(data, slots[idx], len);
            }
        }

        bool HighPrefixCmp(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            const auto *header = VarlenHeader::CastHeader(data);
            if (header->max_val == UNDEFINED_OFFSET)
            {
                return false;
            }
            const auto max_val = SlotValLeaf<ValTyp>(data, header->max_val);
            const auto main_val = SlotValLeaf<ValTyp>(key);
            int cmp = CmpPrefix(max_val, main_val);
            // DB7_ASSERT(cmp != 0, "Can not have value to be 0 when inserting the tree");
            return cmp == 0;
        }

    public:
        static constexpr page_id UNDEFINED_PAGE = std::numeric_limits<page_id>::max();
        static constexpr u32 UNDEFINED_OFFSET = std::numeric_limits<u16>::max();

        BtreeVarlenLayoutLeaf() {}

        ResultObj<void> Get(byte *data, const u16 count, const Key key, shared::VectorValues<ValTyp> &results)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            u16 *slots = CastSlots(data);

            const auto main_val = SlotValLeaf<ValTyp>(key);
            int idx = FindInsertPosition(data, slots, main_val, count);

            std::vector<ValTyp> &result = results.vec;
            int i;
            for (i = idx; i < int(count); i++)
            {
                auto cur = SlotValLeaf<ValTyp>(data, slots[i]);
                int cmp = CmpPrefix(cur, main_val);
                if (cmp != 0)
                {
                    break;
                }
                result.push_back(cur.Result());
            }

            results.proceed = i == int(count); // TODO fix index

            return ResultObj<void>::Ok();
        }

        ResultObj<void> Insert(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");
            DB7_ASSERT(key.len < DB7_MAX_ROW_SIZE, "should be checked in the binder");

            u16 tuple_heap_offset = AppendHeap(data, key);

            InsertSlot(data, tuple_heap_offset);

            return ResultObj<void>::Ok();
        }

        bool HasSpace(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            const auto *header = VarlenHeader::CastHeader(data);
            const u16 count = header->count;
            const u16 slot_size = (count + 1) * sizeof(u16);
            const u16 prev_heap_offset = header->heap_offset;
            const u16 new_heap_offset = CalculateHeapOffset(prev_heap_offset, key.len);
            return new_heap_offset >= slot_size + header_size_;
        }

        bool HasSplit(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            const auto *header = VarlenHeader::CastHeader(data);
            if (header->max_val == UNDEFINED_OFFSET)
            {
                return false;
            }
            const auto max_val = SlotValLeaf<ValTyp>(data, header->max_val);
            const auto main_val = SlotValLeaf<ValTyp>(key);
            int cmp = CmpFull(max_val, main_val);
            // DB7_ASSERT(cmp != 0, "Can not have value to be 0 when inserting the tree");
            return cmp <= 0;
        }

        /**
         * TODO might be better to use thread local buffer for this case
         * to avoid copying objects
         */
        ResultObj<Key> Split(byte *__restrict left_data, byte *__restrict right_data, ValTyp new_pid, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
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
            auto sentinel = SlotValLeaf<ValTyp>(right_data, right_slots[0]);
            u16 left_max = AppendHeap(left_data, {sentinel.len, sentinel.data});

            // update headers
            right_header->rlink = left_header->rlink;
            right_header->count = left_header->count - split;
            right_header->max_val = right_max;

            left_header->rlink = new_pid;
            left_header->count = split;
            left_header->max_val = left_max;

            // insert key
            auto main_val = SlotValLeaf<ValTyp>(key);
            int cmp = CmpFull(sentinel, main_val);
            if (cmp <= 0)
            {
                // insert right
                DB7_ASSERT(HasSpace(right_data, key), "new key does not fit in right half after split");
                u16 tuple_heap_offset = AppendHeap(right_data, key);
                InsertSlot(right_data, tuple_heap_offset);
            }
            else
            {
                // insert left
                DB7_ASSERT(HasSpace(left_data, key), "new key does not fit in left half after split");
                u16 tuple_heap_offset = AppendHeap(left_data, key);
                InsertSlot(left_data, tuple_heap_offset);
            }

            return ResultObj<Key>({sentinel.len, sentinel.data}, true);
        }

        ResultObj<void> Delete(byte *data, Key key)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            u16 &count = VarlenHeader::CastHeader(data)->count;
            u16 *slots = CastSlots(data);

            const auto main_val = SlotValLeaf<ValTyp>(key);
            int idx = FindDeletePosition(data, slots, main_val, count);
            auto cur = SlotValLeaf<ValTyp>(data, slots[idx]);
            int cmp = CmpFull(cur, main_val);
            DB7_ASSERT(cmp == 0, "Unreachable. Key is not found. This should only be called by GC. GC tried to delete non existing value");
            ShiftLeftDelete(slots, idx, count);
            count--;
            return ResultObj<void>::Ok();
        }

        ResultObj<bool> CheckUnique(transaction::TransactionContext *txn, byte *data, Key key, int idx = -1)
        {
            DB7_ASSERT(key.data != nullptr, "invalid key");
            DB7_ASSERT(key.len != 0, "invalid key");

            u16 count = VarlenHeader::CastHeader(data)->count;
            u16 *slots = CastSlots(data);
            const auto main_val = SlotValLeaf<ValTyp>(key);
            if (idx == -1)
            {
                idx = FindStartPosition(data, slots, main_val, count);
            }

            int i;
            for (i = idx; i < count; i++)
            {
                auto cur = SlotValLeaf<ValTyp>(data, slots[i]);
                int cmp = CmpPrefix(cur, main_val);
                if (cmp != 0)
                {
                    break;
                }

                ValTyp tid = cur.Result();
                if (txn->HasUniqueConflict(tid))
                {
                    return ResultObj<bool>::Fail();
                }
            }

            return ResultObj<bool>(true, HighPrefixCmp(data, key));
        }
    };
}