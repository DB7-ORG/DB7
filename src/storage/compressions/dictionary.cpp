#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>

struct MapEntry
{
    u32 hash;
    u32 value;
    u8 *key;
    u16 key_len;
};

struct HMap
{
    MapEntry *entries;
    size_t hash_capacity;

    HMap(size_t count);
    ~HMap();
    u32 get_insert(u8 *key, u16 len, u32 value);
};

#include <sys/mman.h>
HMap::HMap(size_t count)
{
    size_t target = count * 2;
    if (target == 0)
    {
        hash_capacity = 1;
    }
    else if (target == 1)
    {
        hash_capacity = 1;
    }
    else
    {
        int leading_zeros = __builtin_clzll(target - 1);
        hash_capacity = 1ULL << (64 - leading_zeros);
    }

    entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

    // (MapEntry *)mmap(
    //     NULL,
    //     hash_capacity * sizeof(MapEntry),
    //     PROT_READ | PROT_WRITE,
    //     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, // Pre-fault pages
    //     -1, 0);

    // entries = (MapEntry *)malloc(hash_capacity * sizeof(MapEntry));
    // memset(entries, 0, hash_capacity * sizeof(MapEntry));
}

HMap::~HMap()
{
    free(entries);
}

inline u32 HMap::get_insert(u8 *key, u16 len, u32 value)
{
    u32 hash = XXH32(key, len, 0);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (data.key == NULL)
        { // empty slot
            data = MapEntry{
                hash,
                value,
                key,
                len};

            return value;
        }
        else if (data.key != NULL &&
                 data.hash == hash &&
                 data.key_len == len &&
                 memcmp(data.key, key, len) == 0)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

inline int get_bits_used(u32 value)
{
    if (value == 0)
        return 0;
    return 32 - __builtin_clz(value);
}

u32 *serialize(u32 *out, u32 *indexes, u8 *strings, u32 *data, u32 idx, u32 count)
{
    // serialization
    u32 usedBits = get_bits_used(idx);
    out[0] = usedBits;
    out++;
    u32 *initidx = (u32 *)BitPackEncoder::scalar_encode(out, data, count, usedBits);

    initidx[0] = idx;
    initidx++;

    u32 usedBitsIdx = get_bits_used(indexes[idx - 1]);
    initidx[0] = usedBitsIdx;
    initidx++;
    u32 *initstr = (u32 *)BitPackEncoder::scalar_encode(initidx, indexes, idx, usedBitsIdx);

    u32 strsize = indexes[idx - 1];
    memcpy(initstr, strings, strsize);

    return initstr + strsize;
}

u32 *DictionaryStringEncoder::encode(u32 *out, u8 **in, u32 *lenIn, u32 count, u32 strLen)
{
    // TODO should be passed as parameter to function
    u8 *strings = (u8 *)malloc(strLen);                      // TODO this is not len i want, should use vec w allocators
    u32 *indexes = (u32 *)malloc((count + 1) * sizeof(u32)); // TODO this is not len i want, should use vec w allocators
    u32 *data = (u32 *)malloc(count * sizeof(u32));

    indexes[0] = 0;
    u32 idx = 1;

    HMap map(count);

    for (u32 i = 0; i < count; i++)
    {
        u32 len = lenIn[i];
        u8 *key = in[i];
        u32 item = map.get_insert(key, len, idx);
        data[i] = item;
        if (item == idx)
        {
            indexes[idx] = indexes[idx - 1] + len;
            memcpy(strings + indexes[idx - 1], key, len);
            idx++;
        }
    }

    return serialize(out, indexes, strings, data, idx, count);
}

u32 *DictionaryStringEncoder::decode(u8 **out, u32 *lenOut, const u32 *in, u32 count)
{
    u32 *indexes = (u32 *)malloc((count + 1) * sizeof(u32)); // TODO this is not len i want, should use vec w allocators
    u32 *data = (u32 *)malloc(count * sizeof(u32));

    u32 usedBits = *(in++);
    BitPackEncoder::scalar_decode(data, in, count, usedBits); // TODO better cache usage
                                                              // when doing block compression i should
                                                              // resuse data while its in l1 cache
    const u32 *initidx = in + words_used(count, usedBits);

    u32 idxcount = *(initidx++);
    u32 usedBitsIdx = *(initidx++);
    BitPackEncoder::scalar_decode(indexes, initidx, idxcount, usedBitsIdx);
    u8 *initstr = (u8 *)(initidx + words_used(idxcount, usedBitsIdx));

    for (u32 i = 0; i < count; i++)
    {
        u32 idx = data[i];
        u32 start = indexes[idx - 1];
        u32 end = indexes[idx];
        out[i] = initstr + start;
        lenOut[i] = end - start;
    }

    u32 total_string_size = indexes[idxcount - 1];
    return (u32 *)(initstr + total_string_size);
}

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::encode(ValueType *out, ValueType *in, u32 count)
{
    u16 size = sizeof(ValueType);
    ValueType *data = (ValueType *)malloc(count * size);
    ValueType *values = (ValueType *)malloc(count * size);

    HMap map(count);
    u32 idx = 1;

    for (u32 i = 0; i < count; i++)
    {
        ValueType key = in[i];
        u32 item = map.get_insert(key, size, idx);
        data[i] = item;
        if (item == idx)
        {
            values[idx - 1] = item;
            idx++;
        }
    }

    // TODO serialize
}

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::decode(ValueType *out, const ValueType *in, u32 count)
{
}