#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // uint32_t, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>
#include <x86intrin.h>

struct DictEncodedRes
{
    uint32_t *encoded;
    uint32_t *indexes;
    char *strings;
    size_t unique_str;
    size_t count;
};

struct DictionaryEncoder
{
    static DictEncodedRes encode(size_t count, uint8_t **in, size_t *lenIn, uint32_t *out);
    // static void decode();
};

struct BitPackEncoder
{
    static int encode(void *out, void *in, uint32_t nitems, uint32_t usedBits);
    static int decode(void *out, void *in, uint32_t nitems, uint32_t usedBits);
    static uint32_t decode_single(const void *compressed, uint32_t idx, uint32_t usedBits);
};

// struct SymbolTable
// {
//     uint16_t nSymbols;
//     uint16_t sIndex[257];
//     std::string symbols[512];

//     SymbolTable();
//     void compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text);
//     uint16_t findLongestSymbol(std::string &text, uint32_t pos);
//     SymbolTable makeTables(uint16_t count1[512], uint16_t count2[512][512]);
//     void insert(std::string &s);
//     void makeIndex();
// };

// SymbolTable buildSymbolTable(std::string &text);

#endif