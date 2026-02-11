#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "../../shared/append_hmap.h"
#include <string.h>

template <typename ValueType>
inline int get_bits_used(ValueType value)
{
    if (value == 0)
        return 0;
    return sizeof(ValueType) * 8 - __builtin_clz(value);
}

struct DictionaryStringEncodedRes
{
    u32 *codes;
    u32 *indexes;
    u8 *strings;
};

struct DictionaryStringEncoder
{
    static void encode(DictionaryStringEncodedRes *out, u8 **in, u32 *lenIn, u32 count);
    static void decode(u8 **out, u32 *lenOut, const DictionaryStringEncodedRes *in, u32 count);
};

template <typename ValueType>
struct DictionaryValueEncodedRes
{
    ValueType *codes;
    ValueType *values;
    u32 valCount;
};

template <typename ValueType>
struct DictionaryValueEncoder
{
    static void encode(DictionaryValueEncodedRes<ValueType> *out, ValueType *in, const u32 count);
    static void decode(ValueType *out, const DictionaryValueEncodedRes<ValueType> *in, const u32 count);
};

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::encode(DictionaryValueEncodedRes<ValueType> *out, ValueType *in, u32 count)
{
    HMap<ValueType> map(count);
    u32 idx = 1;

    for (u32 i = 0; i < count; i++)
    {
        ValueType key = in[i];
        u32 item = map.get_insert(key, idx);
        out->codes[i] = item - 1;
        if (item == idx)
        {
            out->values[idx - 1] = key;
            idx++;
        }
    }
    out->valCount = idx - 1;
}

template <typename ValueType>
void DictionaryValueEncoder<ValueType>::decode(ValueType *out, const DictionaryValueEncodedRes<ValueType> *in, const u32 count)
{
    const ValueType *codes = in->codes;
    const ValueType *values = in->values;

    for (u32 i = 0; i < count; i++)
    {
        ValueType idx = codes[i];
        ValueType value = values[idx];
        out[i] = value;
    }
}

#endif