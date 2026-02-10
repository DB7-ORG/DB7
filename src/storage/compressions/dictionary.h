#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "common.h"
#include "../../shared/append_hmap.h"
#include <string.h>

template <typename ValueType>
inline int get_bits_used(ValueType value)
{
    if (value == 0)
        return 0;
    return sizeof(ValueType) * 8 - __builtin_clz(value);
}

struct DictionaryStringEncoder
{
    static u32 *encode(u32 *out, u8 **in, u32 *lenIn, u32 count, u32 strLen);
    static u32 *decode(u8 **out, u32 *lenOut, const u32 *in, u32 count);
};

template <typename ValueType>
struct DictionaryValueEncoder
{
    static void encode(ValueType *out, ValueType *in, u32 count);
    static void decode(ValueType *out, const ValueType *in, u32 count);
};

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::encode(ValueType *out, ValueType *in, u32 count)
{
    u32 size = sizeof(ValueType);
    ValueType *data = (ValueType *)malloc(count * size);
    ValueType *values = (ValueType *)malloc(count * size);

    HMap<ValueType> map(count);
    u32 idx = 1;

    for (u32 i = 0; i < count; i++)
    {
        ValueType key = in[i];
        u32 item = map.get_insert(key, idx);
        data[i] = item;
        if (item == idx)
        {
            values[idx - 1] = key;
            idx++;
        }
    }

    memcpy(out, data, count * size);
    memcpy(out + count, values, (idx - 1) * size);
}

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::decode(ValueType *out, const ValueType *in, u32 count)
{
    const ValueType *data = in;
    const ValueType *values = in + count;

    for (u32 i = 0; i < count; i++)
    {
        ValueType idx = data[i];
        ValueType value = values[idx - 1];
        out[i] = value;
    }
}

#endif