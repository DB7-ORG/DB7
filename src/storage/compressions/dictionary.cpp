#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/mman.h>

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

    HMap<u8 *> map(count);

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