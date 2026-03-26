#pragma once

#include "common.hpp"

#include "slab_arena.hpp"

namespace db7
{
    template <typename Key, typename Value>
    struct Entry
    {
        u32 hash;
        Key key;
        Value value;
        u32 next;

        Entry() = default;
        Entry(u32 h, Key k, Value v, i32 n)
            : hash(h), key(std::move(k)), value(std::move(v)), next(n) {}
    };

    template <typename Key, typename Value>
    class HashTable
    {
        u32 *buckets;
        Entry<Key, Value> **entries;
        SlabArena *arena;
        u32 size;
        u32 capacity;
        u32 bucket_num;

    public:
        HashTable(u32 capacity, double ratio, SlabArena *arena)
            : arena(arena), size(0), capacity(capacity), bucket_num(capacity * ratio)
        {
            buckets = arena->Alloc<u32>(bucket_num);
            entries = arena->Alloc<Entry<Key, Value> *>(capacity);

            for (u32 i = 0; i < bucket_num; i++)
            {
                buckets[i] = 0;
            }
        }

        HashTable(const HashTable &) = default;
        HashTable &operator=(const HashTable &) = default;

        void add(Key key, Value val)
        {
            if (size == capacity)
            {
                resize();
            }

            u32 hash = hashIt(key);

            internalAdd(hash, key, val);
        }

        void printKeys(Key key)
        {
            u32 hash = hashIt(key);
            u32 bucket = hash % bucket_num;

            u32 idx = buckets[bucket];
            if (idx == 0)
            {
                return;
            }

            do
            {
                auto entry = entries[idx - 1];
                idx = entry->next;
                if (entry->hash == hash && entry->key == key)
                {
                    std::cout << entry->value << " ";
                }
            } while (idx != 0);

            std::cout << '\n';
        }

    private:
        void addEntry(u32 hash, Key key, Value val, u32 idx)
        {
            entries[idx] = arena->New<Entry<Key, Value>>(hash, key, val, 0);
        }

        u32 hashIt(Key key)
        {
            return (u32)key; // TODO key
        }

        void resize()
        {
            auto newHTable = HashTable(capacity * 2, 1.33, arena);

            for (u32 i = 0; i < size; i++)
            {
                auto entry = entries[i];
                newHTable.internalAdd(entry->hash, entry->key, entry->value);
            }

            *this = newHTable;
        }

        void internalAdd(u32 hash, Key key, Value val)
        {
            u32 bucket = hash % bucket_num;
            u32 free_slot = ++size;
            u32 idx = buckets[bucket];
            if (idx == 0)
            {
                buckets[bucket] = free_slot;
                addEntry(hash, key, val, free_slot - 1);
            }
            else
            {
                Entry<Key, Value> *entry;
                do
                {
                    entry = entries[idx - 1];
                    idx = entry->next;
                } while (idx != 0);

                entry->next = free_slot;
                addEntry(hash, key, val, free_slot - 1);
            }
        }
    };
}