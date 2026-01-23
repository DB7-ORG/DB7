#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <unistd.h>
#include <unordered_map>
#include <iostream>

struct DictionaryEncoded
{
    int *encoded;
    size_t num_items;
    std::unordered_map<std::string_view, int> encoding_map;
};

DictionaryEncoded dictionaryEncode(char *buffer, size_t size, size_t item_size);

#endif