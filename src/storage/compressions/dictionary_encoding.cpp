#include "compression.h"

DictionaryEncoded dictionaryEncode(char *buffer, size_t size, size_t item_size)
{
    size_t num = size / item_size;
    std::unordered_map<std::string_view, int> encoding_map;
    int *encoded = (int *)malloc(num * sizeof(int));

    for (size_t i = 0; i < num; i++)
    {
        std::string_view window(buffer + i * item_size, item_size);

        if (auto it = encoding_map.find(window); it != encoding_map.end())
        {
            encoded[i] = it->second;
        }
        else
        {
            int code = static_cast<int>(encoding_map.size());
            encoding_map[window] = code;
            encoded[i] = code;
        }
    }

    return DictionaryEncoded{
        encoded,
        num,
        encoding_map,
    };
}

// template <typename T>
// int decode()
// {
// }
