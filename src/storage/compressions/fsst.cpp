#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <string.h>

struct SymbolTable
{
    uint16_t nSymbols;
    uint16_t sIndex[257];
    std::string symbols[512];

    SymbolTable();
    void compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text);
    uint32_t findLongestSymbol(std::string &text, uint32_t pos);
};

SymbolTable::SymbolTable()
{
    nSymbols = 0;
    memset(sIndex, 0, sizeof(sIndex)); // TODO not sure if i need this
    for (uint8_t i = 0; i < 255; i++)
    {
        symbols[i] = i;
    }
}

// def findLongestSymbol(st, text):
//  var letter = ord(text[0])
//  # try all symbols that start with this letter
//  for code in range(st.sIndex[letter],st.sIndex[letter+1])
//      if (text.startswith(st.symbols[code]))
//          return code # symbol, code >= 256
// return letter # non-symbol byte (will be escaped)

uint32_t SymbolTable::findLongestSymbol(std::string &text, uint32_t pos)
{
    char letter = text[0];
    for (uint32_t code = sIndex[letter]; code < sIndex[letter + 1]; code++)
    {
        if (text.compare(0, symbols[code].length(), symbols[code]) == 0)
        {
            return code;
        }
    }
    return letter;
}

void SymbolTable::compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text)
{
    uint32_t pos = 0;
    uint32_t code = findLongestSymbol(text, pos);
    uint32_t prev = 0;
    pos += symbols[code].length();
    while (pos < text.length())
    {
        prev = code;
        code = findLongestSymbol(text, pos);
        count1[code]++;
        count2[prev][code]++;
        if (code >= 256)
        {
            char nextByte = text[pos];
            count1[nextByte]++;
            count2[prev][nextByte]++;
        }
        pos += symbols[code].length();
    }
}

void buildSymbolTable(std::string &text) // TODO consider char*
{
    SymbolTable st;
    for (uint8_t gen = 1; gen <= 5; gen++)
    {
        uint16_t count1[512] = {};
        uint16_t count2[512][512] = {};
        st.compressCount(count1, count2, text);
        // res = st.makeTables(res,count1, count2);
    }
}

struct FsstEncoder
{
};