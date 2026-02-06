#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>

struct MapEntry
{
    u32 hash;
    u32 value;
    u8 *key;
    u16 key_len;
};

struct HMap
{
    MapEntry *entries;
    size_t hash_capacity;

    HMap(size_t count);
    ~HMap();
    u32 get_insert(u8 *key, u16 len, u32 value);
};

#include <sys/mman.h>
HMap::HMap(size_t count)
{
    size_t target = count * 2;
    if (target == 0)
    {
        hash_capacity = 1;
    }
    else if (target == 1)
    {
        hash_capacity = 1;
    }
    else
    {
        int leading_zeros = __builtin_clzll(target - 1);
        hash_capacity = 1ULL << (64 - leading_zeros);
    }

    entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

    // (MapEntry *)mmap(
    //     NULL,
    //     hash_capacity * sizeof(MapEntry),
    //     PROT_READ | PROT_WRITE,
    //     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, // Pre-fault pages
    //     -1, 0);

    // entries = (MapEntry *)malloc(hash_capacity * sizeof(MapEntry));
    // memset(entries, 0, hash_capacity * sizeof(MapEntry));
}

HMap::~HMap()
{
    free(entries);
}

inline u32 HMap::get_insert(u8 *key, u16 len, u32 value)
{
    u32 hash = XXH32(key, len, 0);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (data.key == NULL)
        { // empty slot
            data = MapEntry{
                hash,
                value,
                key,
                len};

            return value;
        }
        else if (data.key != NULL &&
                 data.hash == hash &&
                 data.key_len == len &&
                 memcmp(data.key, key, len) == 0)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

u32 *DictionaryEncoder::encode(u32 *out, u8 **in, u32 *lenIn, u32 count, u32 strLen)
{
    u8 *strings = (u8 *)malloc(strLen);                      // TODO this is not len i want, should use vec w allocators
    u32 *indexes = (u32 *)malloc((count + 1) * sizeof(u32)); // TODO this is not len i want, should use vec w allocators
    indexes[0] = 0;
    u32 idx = 1;

    HMap map(count);

    for (size_t i = 0; i < count; i++)
    {
        u32 len = lenIn[i];
        u8 *key = in[i];
        u32 data = map.get_insert(key, len, idx);
        out[i] = data;
        if (data == idx)
        {
            indexes[idx] = indexes[idx - 1] + len;
            memcpy(strings + indexes[idx - 1], key, len);
            idx++;
        }
    }

    // serialization
    u32 *initidx = out + count;
    initidx[0] = idx;
    initidx++;
    memcpy(initidx, indexes, idx * sizeof(u32));
    u32 *initstr = initidx + idx;
    u32 strsize = indexes[idx - 1];
    memcpy(initstr, strings, strsize);

    return initstr + strsize;
}

u32 *DictionaryEncoder::decode(u8 **out, u32 *lenOut, const u32 *in, u32 count)
{
    const u32 *initdata = in;
    const u32 *initidx = initdata + count;
    u32 idxcount = *(initidx++);
    u8 *initstr = (u8 *)(initidx + idxcount);

    for (u32 i = 0; i < count; i++)
    {
        u32 idx = initdata[i];
        u32 start = initidx[idx - 1];
        u32 end = initidx[idx];
        out[i] = initstr + start;
        lenOut[i] = end - start;
    }

    u32 total_string_size = initidx[idxcount - 1];
    return (u32 *)(initstr + total_string_size);

    // u32 blocks = count / 8;
    // const u32 *initdata = in;
    // const u32 *initidx = initdata + count;
    // u32 idxcount = *(initidx++);
    // u8 *initstr = (u8 *)(initidx + idxcount);

    // const __m256i *siminitdata = (const __m256i *)initdata;
    // // const __m256i *siminitidx = (const __m256i *)initidx;
    // //  auto siminitdata = (__m256i *)initdata;

    // for (u32 i = 0; i < blocks; i++)
    // {
    //     __m256i idx = _mm256_loadu_si256(siminitdata + i);
    //     __m256i idx_minus_1 = _mm256_sub_epi32(idx, _mm256_set1_epi32(1));

    //     __m256i starts = _mm256_i32gather_epi32((int *)initidx, idx_minus_1, 4);
    //     __m256i ends = _mm256_i32gather_epi32((int *)initidx, idx, 4);
    //     __m256i lengths = _mm256_sub_epi32(ends, starts);

    //     _mm256_storeu_si256((__m256i *)&lenOut[i * 8], lengths);

    //     u32 start_vals[8];
    //     _mm256_storeu_si256((__m256i *)start_vals, starts);

    //     for (int j = 0; j < 8; j++)
    //     {
    //         out[i * 8 + j] = initstr + start_vals[j];
    //         // std::cout << std::string_view((const char *)(out[i * 8 + j]), lenOut[i * 8 + j]) << std::endl;
    //     }
    // }

    // return nullptr;
}

// constexpr u64 HASH_NUM_1 = 14695981039346656037ULL;
// constexpr u64 HASH_NUM_2 = 1099511627776ULL;

// u64 DictionaryEncoder::hash_fnv1a(const u8 *val, size_t size)
// {
//     u64 hash = HASH_NUM_1;
//     u64 val64 = *((u64 *)val);
//     u64 shift_amt = ((8 - size) & 7) * 8;
//     val64 = (val64 << shift_amt) >> shift_amt;
//     hash ^= val64;
//     hash *= HASH_NUM_2;
//     return hash;
// }

// void DictionaryEncoder::hash_fnv1a_simd(const u8 **val, u64 *size, __m256i *out)
// {

//     auto hash = _mm256_set1_epi64x(HASH_NUM_1);
//     auto data = _mm256_set_epi64x(
//         *((u64 *)val[3]),
//         *((u64 *)val[2]),
//         *((u64 *)val[1]),
//         *((u64 *)val[0]));

//     auto size_vec = _mm256_lddqu_si256((__m256i *)size);
//     auto init_vec = _mm256_set1_epi64x(8);
//     auto shift_amt = _mm256_sub_epi64(init_vec, size_vec);
//     auto mod_vec = _mm256_set1_epi64x(7);
//     shift_amt = _mm256_and_si256(shift_amt, mod_vec);
//     auto sh_vec = _mm256_set1_epi64x(3);
//     shift_amt = _mm256_sllv_epi64(shift_amt, sh_vec);

//     __m256i result = _mm256_sllv_epi64(data, shift_amt);
//     result = _mm256_srlv_epi64(result, shift_amt);
//     result = _mm256_xor_si256(result, hash);
//     result = _mm256_slli_epi64(result, 40);
//     _mm256_storeu_si256(out, result);
// }
