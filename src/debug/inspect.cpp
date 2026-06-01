#include "debug/inspect.hpp"

#ifdef DEBUG

#include <cstring>
#include <limits>

namespace db7::debug
{
    extern "C" PageDump *InspectVarlenLayout(byte *data)
    {
        static PageDump dump{};
        dump = {};
        auto *header = reinterpret_cast<access::VarlenHeader *>(data);

        dump.pid = header->pid;
        dump.rlink = header->rlink;
        dump.llink = header->llink;
        dump.count = header->count;
        dump.level = header->level;
        dump.max_val = header->max_val;
        dump.heap_size = header->heap_size;

        bool is_leaf = header->level == 0;

        // max_val key
        u64 undefined = is_leaf
                            ? std::numeric_limits<u64>::max()
                            : std::numeric_limits<page_id>::max();

        if (header->max_val != undefined)
        {
            byte *ptr = data + header->max_val;
            u16 len = 0;
            char *key_data = nullptr;

            if (is_leaf)
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<u64> *>(ptr);
                len = hdr->len;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<u64>));
            }
            else
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<page_id> *>(ptr);
                len = hdr->len;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<page_id>));
            }
            u16 copy = std::min(len, (u16)255);
            memcpy(dump.max_val_key, key_data, copy);
            dump.max_val_key[copy] = '\0';
        }
        else
        {
            strcpy(dump.max_val_key, "(UNDEFINED)");
        }

        // slots
        access::Slot *slots = reinterpret_cast<access::Slot *>(data + sizeof(access::VarlenHeader));
        u32 count = std::min(header->count, (u32)1024);

        for (u32 i = 0; i < count; i++)
        {
            byte *ptr = data + slots[i].offset;
            u16 len = 0;
            u64 res = 0;
            char *key_data = nullptr;

            if (is_leaf)
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<u64> *>(ptr);
                len = hdr->len;
                res = hdr->result;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<u64>));
            }
            else
            {
                auto *hdr = reinterpret_cast<access::SlotValHeader<page_id> *>(ptr);
                len = hdr->len;
                res = hdr->result;
                key_data = reinterpret_cast<char *>(ptr + sizeof(access::SlotValHeader<page_id>));
            }

            dump.slots[i].slot = i;
            dump.slots[i].offset = slots[i].offset;
            dump.slots[i].len = len;
            dump.slots[i].result_val = res;
            u16 copy = std::min(len, (u16)255);
            memcpy(dump.slots[i].key, key_data, copy);
            dump.slots[i].key[copy] = '\0';
        }

        return &dump;
    }
}

#endif