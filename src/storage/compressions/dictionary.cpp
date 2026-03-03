#include "dictionary.hpp"

#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/mman.h>

void DictionaryValueEncoder::EstimateCompression(const u32 nunique, const u32 nitems, const u8 sizeOfType, u32 &codeSize, u32 &valueSize)
{
    codeSize = nitems * sizeOfType;
    valueSize = nunique * sizeOfType;
};

void DictionaryStringEncoder::Encode(DictionaryStringEncodedRes *out, u8 **in, const u32 *lenIn, const ValidityMask *nullmap, const u32 count)
{
    u8 *strings = out->strings;
    u32 *indexes = out->indexes;
    u32 *codes = out->codes;

    indexes[0] = 0;
    u32 idx = 1;

    AppendOnlyStrHMap map(count, 2);

    bool allValid = nullmap->AllValid();

    for (u32 i = 0; i < count; i++)
    {
        bool isValid = allValid || nullmap->RowIsValid(i);
        if (!isValid)
        {
            out->codes[i] = 0;
            continue;
        }

        auto key = StringKey{
            in[i],
            (u16)lenIn[i], // TODO
        };
        u32 item = map.GetInsert(key, idx);
        codes[i] = item;
        if (item == idx)
        {
            indexes[idx] = indexes[idx - 1] + key.len;
            memcpy(strings + indexes[idx - 1], key.ptr, key.len);
            idx++;
        }
    }
}

void DictionaryStringEncoder::Decode(u8 **out, u32 *lenOut, const DictionaryStringEncodedRes *in, u32 count)
{
    const u32 *indexes = in->indexes;
    const u32 *codes = in->codes;
    u8 *strings = in->strings;

    for (u32 i = 0; i < count; i++)
    {
        u32 idx = codes[i];
        u32 start = indexes[idx - 1];
        u32 end = indexes[idx];
        out[i] = strings + start;
        lenOut[i] = end - start;
    }
}