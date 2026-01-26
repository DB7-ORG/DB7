#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // uint32_t, uint16_t, int32_t, etc.
#include <cstddef>
#include <string.h>
#include <string>

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
    static DictEncodedRes encode(char *buffer, size_t byte_size, size_t count);
    // static void decode();
};

struct SymbolTable
{
    uint16_t nSymbols;
    uint16_t sIndex[257];
    std::string symbols[512];

    SymbolTable();
    void compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text);
    uint16_t findLongestSymbol(std::string &text, uint32_t pos);
    SymbolTable makeTables(uint16_t count1[512], uint16_t count2[512][512]);
    void insert(std::string &s);
    void makeIndex();
};

SymbolTable buildSymbolTable(std::string &text);

#endif