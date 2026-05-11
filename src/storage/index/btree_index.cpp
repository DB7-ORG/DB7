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

    u32 CopyUpperHalf(T *from, T *to, u32 count, bool isLeaf)
    {
        u32 mid = (count + 1) / 2;
        u32 inc = isLeaf ? 1 : 0;
        std::memcpy(to, from + mid + inc, (count - mid) * sizeof(T));
        return mid;
    }

    template <typename Typ>
    void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
    {
        std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(T));
        data[idx] = value;
    }

    Page *BTreeIndex::GetNode(PageIdentifier id_)
    {
        auto *page = buffer_pool_->Pin(id_);
        page->WaitIO();
        page->WDataLock();
        return page;
    }

    Page *BTreeIndex::ReserveNode(table_id id_, page_id &pid)
    {
        auto *page = buffer_pool_->Reserve(id_, pid);
        page->WDataLock();
        return page;
    }

    void BTreeIndex::ReleasePage(Page *page)
    {
        page->WDataUnlock();
        buffer_pool_->Unpin(page);
    }

    byte *OffsetHeader(byte *data)
    {
        return data + sizeof(BtreeHeader);
    }

    BtreeHeader *GetHeader(byte *data)
    {
        return reinterpret_cast<BtreeHeader *>(data);
    }

    void WriteHeader(BtreeHeader *header, byte *data)
    {
        std::memcpy(data, header, sizeof(BtreeHeader));
    }

    void BTreeIndex::Split(BtreeHeader *header, byte *data)
    {
        page_id new_pid;
        auto *right_page = ReserveNode(tbl_id_, new_pid);

        byte *new_node_data = right_page->GetData();

        u32 mid = CopyUpperHalf((T *)data, (T *)new_node_data, header->count, header->level == 0);

        auto new_header = BtreeHeader(header->rlink, header->count - mid, header->level, header->max_val);

        auto node_header = BtreeHeader(new_pid, mid, header->level, ((T *)data)[mid]);

        WriteHeader(&new_header, new_node_data);

        WriteHeader(&node_header, data);
    }

    Page *BTreeIndex::DropToLevel(std::vector<page_id> &state, T key, u8 drop_level)
    {
        page_id pid = root_id_;
        while (true)
        {
            auto *page = GetNode(PageIdentifier(pid, tbl_id_));
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
                auto *data = OffsetHeader(page->GetData());
                u32 idx = FindPosition((T *)data, header->count, key);
                pid = reinterpret_cast<page_id *>(data + MAX_COUNT)[idx];
            }
            ReleasePage(page);
        }
    }

    bool BTreeIndex::InsertInternal(std::vector<page_id> &state, Page *page, T key, R value)
    {
        BtreeHeader *header = GetHeader(page->GetData());
        while (header->max_val != UNDEFINED && key >= header->max_val)
        {
            page_id pid = header->rlink;
            ReleasePage(page);
            page = GetNode(PageIdentifier(pid, tbl_id_));
            header = GetHeader(page->GetData());
        };

        auto *data = OffsetHeader(page->GetData());
        if (header->count < MAX_COUNT)
        {
            u32 idx = FindPosition((T *)data, header->count, key);
            ShiftRightInsert((T *)data, header->count, idx, key);
            ShiftRightInsert((R *)(data + REF_OFFSET), header->count, idx + 1, value);
            header->count++;
            WriteHeader(header, page->GetData());
        }
        else
        {
            // TODO fix split code
            Split(header, data);
            // propagate insert
        }
        return true;
    }

    bool BTreeIndex::Insert(T key, R value)
    {
        std::vector<page_id> state(10);
        auto *page = DropToLevel(state, key);
        return InsertInternal(state, page, key, value);
    }

}