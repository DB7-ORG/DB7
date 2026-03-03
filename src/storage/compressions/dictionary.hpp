#pragma once

#include "append_str_hmap.hpp"
#include "append_valtyp_hmap.hpp"
#include "nullbitmap.hpp"
#include "bit_utils.hpp"
#include "align_utils.hpp"

#include <string.h>

template <typename ValueType>
inline int GetBitsUsed(ValueType value)
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
    static void Encode(DictionaryStringEncodedRes *out, u8 **in, const u32 *lenIn, const ValidityMask *nullmap, const u32 count);
    static void Decode(u8 **out, u32 *lenOut, const DictionaryStringEncodedRes *in, u32 count);
};

template <typename ValueType>
struct DictionaryValueEncodedRes
{
    ValueType *codes;
    ValueType *values;
    u32 valCount;
};

struct DictionaryValueEncoder
{
    template <typename ValueType>
    static void Encode(DictionaryValueEncodedRes<ValueType> *out, const ValueType *in, const ValidityMask *nullmap, const u32 count);
    template <typename ValueType>
    static void Decode(ValueType *out, const DictionaryValueEncodedRes<ValueType> *in, const u32 count);
    static void EstimateCompression(const u32 nunique, const u32 nitems, const u8 sizeOfType, u32 &codeSize, u32 &valueSize);
};

template <typename ValueType>
void DictionaryValueEncoder::Encode(
    DictionaryValueEncodedRes<ValueType> *out,
    const ValueType *in,
    const ValidityMask *nullmap,
    const u32 count)
{
    AppendOnlyHMap<ValueType> map(count);
    u32 idx = 1;

    bool allValid = nullmap->AllValid();

    for (u32 i = 0; i < count; i++)
    {
        bool isValid = allValid || nullmap->RowIsValid(i);
        if (!isValid)
        {
            out->codes[i] = 0;
            continue;
        }
        ValueType key = in[i];
        u32 item = map.ScalarGetInsert(key, idx);
        out->codes[i] = item - 1; // TODO add one value for nulls like codes = 0
        if (item == idx)
        {
            out->values[idx - 1] = key;
            idx++;
        }
    }
    out->valCount = idx - 1;
}

template <typename ValueType>
void DictionaryValueEncoder::Decode(
    ValueType *out,
    const DictionaryValueEncodedRes<ValueType> *in,
    const u32 count)
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
