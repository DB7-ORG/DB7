#include "storage/index/btree_index.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>

namespace db7::storage
{
    u32 FindPosition(const T *data, const u32 count, const T value)
    {
        DB7_ASSERT(count != 0, "zero count node");
        T cur;
        u32 i = 0;
        do
        {
            cur = data[i];
            if (cur > value)
            {
                break;
            }
            i++;
        } while (i < count);
        return i;
    }

    template <typename Typ>
    u32 CopyUpperHalf(Typ *from, Typ *to, u32 count, bool isLeaf)
    {
        u32 mid = (count + 1) / 2;
        u32 inc = isLeaf ? 0 : 1;
        std::memcpy(to, from + mid + inc, (count - mid) * sizeof(Typ));
        return mid;
    }

    template <typename Typ>
    void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
    {
        std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
        data[idx] = value;
    }

    Page *BTreeIndex::GetNode(PageIdentifier id_)
    {
        auto *page = buffer_pool_->Pin(id_);
        page->WaitIO();
        page->WDataLock();
        return page;
    }

    Page *BTreeIndex::ReserveNode(table_id id_)
    {
        return buffer_pool_->Reserve(id_);
    }

    void BTreeIndex::ReleaseReservedPage(Page *page)
    {
        buffer_pool_->Unpin(page);
    }

    void BTreeIndex::ReleasePage(Page *page)
    {
        page->WDataUnlock();
        buffer_pool_->Unpin(page);
    }

    byte *OffsetHeader(byte *data)
    {
        return data + KEY_OFFSET;
    }

    BtreeHeader *GetHeader(byte *data)
    {
        return reinterpret_cast<BtreeHeader *>(data);
    }

    void WriteHeader(BtreeHeader *header, byte *data)
    {
        std::memcpy(data, header, sizeof(BtreeHeader));
    }

    page_id BTreeIndex::GetRoot()
    {
        return root_id_.load();
    }

    void NodeInsertInter(byte *data, BtreeHeader *header, T key, page_id value)
    {
        u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
        ShiftRightInsert((T *)OffsetHeader(data), header->count, idx, key);
        ShiftRightInsert((page_id *)(data + REF_OFFSET_INTER), header->count, idx + 1, value);
        header->count++;
        WriteHeader(header, data);
    }

    void NodeInsertLeaf(byte *data, BtreeHeader *header, T key, R value)
    {
        u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
        ShiftRightInsert((T *)OffsetHeader(data), header->count, idx, key);
        ShiftRightInsert((R *)(data + REF_OFFSET_LEAF), header->count, idx + 1, value);
        header->count++;
        WriteHeader(header, data);
    }

    T BTreeIndex::SplitLeaf(BtreeHeader *header, byte *data, page_id &new_pid)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *new_node_data = right_page->GetData();

        u32 mid = CopyUpperHalf((T *)OffsetHeader(data), (T *)new_node_data, header->count, true);

        CopyUpperHalf((R *)(data + REF_OFFSET_LEAF), (R *)(new_node_data + REF_OFFSET_LEAF), header->count, true);

        T sentinel = ((T *)OffsetHeader(data))[mid];

        auto new_header = BtreeHeader(header->rlink, header->count - mid, header->level, header->max_val);

        auto node_header = BtreeHeader(new_pid, mid, header->level, sentinel);

        WriteHeader(&new_header, new_node_data);

        WriteHeader(&node_header, data);

        ReleaseReservedPage(right_page);

        return sentinel;
    }

    T BTreeIndex::SplitInter(BtreeHeader *header, byte *data, page_id &new_pid)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *new_node_data = right_page->GetData();

        u32 mid = CopyUpperHalf((T *)OffsetHeader(data), (T *)new_node_data, header->count, false);

        CopyUpperHalf((page_id *)(data + REF_OFFSET_INTER), (page_id *)(new_node_data + REF_OFFSET_INTER), header->count, false);

        T sentinel = ((T *)OffsetHeader(data))[mid];

        auto new_header = BtreeHeader(header->rlink, header->count - mid, header->level, header->max_val);

        auto node_header = BtreeHeader(new_pid, mid, header->level, sentinel);

        WriteHeader(&new_header, new_node_data);

        WriteHeader(&node_header, data);

        ReleaseReservedPage(right_page);

        return sentinel;
    }

    Page *BTreeIndex::DropToLevel(std::vector<page_id> &state, T key, u8 drop_level)
    {
        page_id pid = GetRoot();
        do
        {
            Page *page = GetNode(PageIdentifier(tbl_id_, pid));
            BtreeHeader *header = GetHeader(page->GetData());
            if (header->level <= drop_level)
            {
                return page;
            }
            else if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                pid = header->rlink;
            }
            else
            {
                state.push_back(pid);
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];
            }
            ReleasePage(page);
        } while (true);

        return nullptr;
    }

    void BTreeIndex::GoRight(Page *&page, BtreeHeader *&header, T key)
    {
        while (header->max_val != UNDEFINED && key >= header->max_val)
        {
            page_id pid = header->rlink;
            ReleasePage(page);
            page = GetNode(PageIdentifier(tbl_id_, pid));
            header = GetHeader(page->GetData());
        };
    }

    void BTreeIndex::CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid)
    {
        Page *new_root_page = ReserveNode(tbl_id_);

        byte *new_root_data = new_root_page->GetData();

        auto h = BtreeHeader(UNDEFINED, 1, level + 1, UNDEFINED);

        WriteHeader(&h, new_root_data);

        *(T *)(new_root_data + KEY_OFFSET) = key;

        *(page_id *)(new_root_data + REF_OFFSET_INTER) = pid;

        *(page_id *)(new_root_data + REF_OFFSET_INTER + sizeof(page_id)) = new_pid;

        root_id_.store(new_root_page->GetPageId());
    }

    bool BTreeIndex::PropagateInsert(std::vector<page_id> &state, T key, page_id value)
    {
        if (state.size() == 0)
        {
            return true;
        }

        while (state.size() > 0)
        {
            page_id pid = state.back();
            state.pop_back();

            auto *page = GetNode(PageIdentifier(tbl_id_, pid));
            BtreeHeader *header = GetHeader(page->GetData());
            GoRight(page, header, key);
            pid = page->GetPageId();

            auto *data = page->GetData();
            if (header->count < MAX_COUNT_INTER)
            {
                NodeInsertInter(data, header, key, value);
                ReleasePage(page);
                return true;
            }

            page_id new_pid;
            key = SplitInter(header, data, new_pid);
            value = new_pid;
            u8 level = header->level;
            ReleasePage(page);

            if (state.empty())
            {
                root_mtx_.lock();
                if (GetRoot() == pid)
                {
                    CreateNewRoot(level, key, pid, new_pid);
                    root_mtx_.unlock();
                    return true;
                }
                else
                {
                    root_mtx_.unlock();
                    DropToLevel(state, key, level + 1);
                    return PropagateInsert(state, key, value);
                }
            }
        }

        return true;
    }

    bool BTreeIndex::InsertInternal(std::vector<page_id> &state, Page *page, T key, R value)
    {
        BtreeHeader *header = GetHeader(page->GetData());
        GoRight(page, header, key);

        auto *data = page->GetData();
        if (header->count < MAX_COUNT_LEAF)
        {
            NodeInsertLeaf(data, header, key, value);
            ReleasePage(page);
        }
        else
        {
            page_id new_pid;
            T sentinel = SplitLeaf(header, data, new_pid);
            ReleasePage(page);
            PropagateInsert(state, sentinel, new_pid);
        }

        return true;
    }

    BTreeIndex::BTreeIndex(BufferPool *buffer_pool, table_id tbl_id)
        : root_id_(1), buffer_pool_(buffer_pool), tbl_id_(tbl_id)
    {
        Page *page = buffer_pool_->Reserve(tbl_id);
        BtreeHeader header(UNDEFINED, 0, 0, UNDEFINED);
        WriteHeader(&header, page->GetData());
        ReleaseReservedPage(page);
    }

    bool BTreeIndex::Insert(T key, R value)
    {
        std::vector<page_id> state;
        state.reserve(4);
        auto *page = DropToLevel(state, key);
        return InsertInternal(state, page, key, value);
    }

}