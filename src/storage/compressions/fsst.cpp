#include "compression.h"

#include <unistd.h>
#include <iostream>

#include <queue>
#include <algorithm>

// struct SymbolTable
// {
//     uint16_t nSymbols;
//     uint16_t sIndex[257];
//     std::string symbols[512];

//     SymbolTable();
//     void compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text);
//     uint8_t findLongestSymbol(std::string &text, uint32_t pos);
//     SymbolTable makeTables(uint16_t count1[512], uint16_t count2[512][512]);
//     void insert(std::string &s);
//     void makeIndex();
// };

// struct Symbol
// {
//     union val
//     {
//         char buf[8];
//         uint64_t num;
//     };
//     uint16_t code;
//     uint16_t ignoredBits;
// };

// struct SymbolTable
// {
//     uint8_t nSymbols;    // # of normal symbols (not counting escapes)
//     Symbol symbols[512]; // all symbols: 0-255 escapes, then n Symbols
//     // uint16_t stores code&length: resp. bits [0..8] and bits [12..15]
//     uint16_t shortCodes[256][256];
//     // codes (511=unused) of 1-2byte symbs
//     static const uint64_t hashTabSize = 4096;
//     Symbol hashTab[hashTabSize]; // keyed on the first three bytes
//     uint64_t hash(uint64_t x);
// };

// void encodeScalar(uint8_t *&cur, uint8_t *&out, SymbolTable &st)
// {
//     uint64_t word = *(uint64_t *)cur;
//     // speculatively write 1st byte (required for escapes, else harmless)
//     out[1] = (uint8_t)word;
//     // lookup in lossy perfect hash table
//     uint64_t idx = st.hash(word & 0xFFFFFF) & (st.hashTabSize - 1);
//     Symbol s = st.hashTab[idx]; // fetch symbol from hash table
//     uint64_t num = word & (0xFFFFFFFFFFFFFFFF >> s.ignoredBits);
//     uint16_t code = (s.val.num == num & s.code != 511) ? s.code : st.shortCodes[word & 0xFFFF]; // conditional move
//     out[0] = (uint8_t)code;                                                            // write out code. Note: (uint8_t) 511=255
//     // advance the pointers with predication (i.e. without branches)
//     out += 2 - ((code >> 8) & 1); // increase with 1 or 2 (escape = 9th bit)
//     cur += (code >> 12);          // symbol length is in bits [12..15] of code
// }

SymbolTable::SymbolTable()
{
    nSymbols = 0;
    memset(sIndex, 256, sizeof(sIndex));

    // TODO not sure if i need this
    for (uint8_t i = 0; i < 255; i++)
    {
        symbols[i] = i;
    }
}

uint16_t SymbolTable::findLongestSymbol(std::string &text, uint32_t pos)
{
    uint16_t letter = text[pos];
    for (uint16_t code = sIndex[letter]; code < sIndex[letter + 1]; code++)
    {
        if (text.compare(pos, symbols[code].length(), symbols[code]) == 0)
        {
            return code;
        }
    }
    return letter;
}

void SymbolTable::compressCount(uint16_t count1[512], uint16_t count2[512][512], std::string &text)
{
    uint32_t pos = 0;
    uint16_t prev;
    uint16_t code = findLongestSymbol(text, pos);
    pos += symbols[code].length();
    while (pos < text.length())
    {
        prev = code;
        code = findLongestSymbol(text, pos);

        count1[code]++;
        count2[prev][code]++;

        if (code >= 256)
        {
            uint8_t nextByte = text[pos];
            count1[nextByte]++;
            count2[prev][nextByte]++;
        }
        pos += symbols[code].length();
    }
}

SymbolTable SymbolTable::makeTables(uint16_t count1[512], uint16_t count2[512][512])
{
    SymbolTable st;
    std::priority_queue<std::pair<double, std::string>> cands;

    for (auto code1 = 0; code1 < 256 + nSymbols; code1++)
    {
        auto gain = symbols[code1].length() * count1[code1];
        cands.push({gain, symbols[code1]});
        for (auto code2 = 0; code2 < 256 + nSymbols; code2++)
        {
            std::string s = (symbols[code1] + symbols[code2]).substr(0, 8);
            gain = s.length() * count2[code1][code2];
            cands.push({gain, s});
        }
    }

    while (st.nSymbols < 255)
    {
        auto el = cands.top();
        if (el.first == 0)
            break;
        std::cout << el.first << " " << el.second << "-" << std::flush;

        cands.pop();
        st.insert(el.second);
    }
    std::cout << std::endl;
    st.makeIndex();
    return st;
}

void SymbolTable::makeIndex()
{
    std::sort(symbols + 256, symbols + 256 + nSymbols);
    for (int i = nSymbols - 1; i >= 0; i--)
    {
        uint8_t letter = (symbols + 256)[i][0];
        sIndex[letter] = 256 + i;
    }
    sIndex[256] = 256 + nSymbols;
}

void SymbolTable::insert(std::string &s)
{
    symbols[256 + nSymbols++] = s;
}

SymbolTable buildSymbolTable(std::string &text) // TODO consider char*
{
    SymbolTable st;
    for (uint8_t gen = 1; gen <= 5; gen++)
    {
        uint16_t count1[512] = {};
        uint16_t count2[512][512] = {};
        st.compressCount(count1, count2, text);
        for (auto c = 1; c < text.length(); c++)
        {
            auto prev = text[c - 1];
            auto ch = text[c];
            std::cout << count1[ch] << "  " << count2[prev][ch] << std::endl
                      << std::flush;
        }
        st = st.makeTables(count1, count2);
    }
    return st;
}

// struct FsstEncoder
// {
//     SymbolTable buildSymbolTable(std::string &text);
// };