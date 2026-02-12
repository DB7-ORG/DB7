#include <iostream>
#include "storage/disk_manager.h"
#include "storage/compressions/compression.h"
#include <random>
#include <string.h>
#include "storage/compressions/fsst.h"
#include <time.h>
#include <cassert>
#include "shared/append_valtyp_hmap.h"

std::mt19937 gen(42);
std::uniform_int_distribution<> status_distribution(1, 100'000);

// int test_read_buffer_enc(const char *filename, long size, const long tuple_num)
// {
//     char *new_buffer = (char *)malloc(size);
//     int read = readCF(filename, new_buffer, size);
//     if (!read)
//     {
//         return 2;
//     }

//     auto val = DictionaryEncoder::encode((char *)new_buffer, read, tuple_num);

//     for (int i = 0; i < 20; i++)
//     {
//         std::cout << val.encoded[i] << " - ";
//     }
//     std::cout << std::endl;

//     for (int i = 0; i < 20; i++)
//     {
//         std::cout << val.indexes[i] << " - ";
//     }

//     free(new_buffer);
//     return 0;
// }

int test_write_data(const char *filename, long size, const long tuple_num, const u16 strsize)
{
    char *buffer = (char *)malloc(size);
    int offset = 0;
    for (int i = 0; i < tuple_num; i++)
    {
        std::string str = "string" + std::to_string(status_distribution(gen));
        memcpy(buffer + offset, &strsize, sizeof(u16));
        memcpy(buffer + offset + 2, str.c_str(), strsize);
        offset += strsize + 2;
    }

    if (!writeCF(filename, (char *)buffer, size))
    {
        return 1;
    }

    free(buffer);
    return 0;
}

void test_print_fsst_compression_results(size_t compressed_total, const size_t lenIn[], const unsigned char **strings, const size_t lenOut[])
{
    printf("\n=== COMPRESSION RESULTS ===\n");
    printf("Total compressed size: %zu bytes\n\n", compressed_total);

    for (int i = 0; i < 20; i++)
    {
        printf("String %d:\n", i);
        printf("  Original (%zu bytes): %s\n", lenIn[i], strings[i]);
        printf("  Compressed (%zu bytes)\n: ", lenOut[i]);
    }
}

void test_print_fsst_decompression_results(fsst_encoder_t *encoder, unsigned char **strOut, const size_t lenIn[], const unsigned char **strings, const size_t lenOut[], size_t count)
{
    // ========== DECOMPRESSION (Your Example) ==========

    // Export the encoder as decoder bytes
    unsigned char decoderBuf[sizeof(fsst_decoder_t)];
    size_t hdr = fsst_export(encoder, decoderBuf);

    printf("\n=== DECOMPRESSION ===\n");
    printf("Decoder header size: %zu bytes\n\n", hdr);

    // For each compressed string, create a buffer with header + compressed data
    for (size_t i = 0; i < count; i++)
    {
        // Create source buffer: [header | compressed_data]
        unsigned char *srcBuf = (unsigned char *)malloc(hdr + lenOut[i]);
        memcpy(srcBuf, decoderBuf, hdr);            // Copy header
        memcpy(srcBuf + hdr, strOut[i], lenOut[i]); // Copy compressed data
        size_t srcLen = hdr + lenOut[i];

        // Allocate destination buffer
        unsigned char *dstBuf = (unsigned char *)malloc(lenIn[i] + 1);

        // Decompress (following your example pattern)
        fsst_decoder_t decoder;
        size_t header_size = fsst_import(&decoder, srcBuf);
        size_t decompressed_len = fsst_decompress(
            &decoder,
            srcLen - header_size, // Compressed data length
            srcBuf + header_size, // Compressed data pointer
            lenIn[i],             // Max output size
            dstBuf                // Output buffer
        );

        dstBuf[decompressed_len] = '\0'; // Null terminate

        printf("String %zu decompressed: %s\n", i, dstBuf);
        printf("  Original length: %zu, Decompressed length: %zu\n", lenIn[i], decompressed_len);

        // Verify
        if (memcmp(strings[i], dstBuf, lenIn[i]) == 0)
        {
            printf("  ✓ Matches original!\n");
        }
        else
        {
            printf("  ✗ ERROR: Doesn't match!\n");
        }
        printf("\n");
    }
}

int fill_data(size_t size, long tuple_num, const u16 strsize, const char *filename)
{
    void *buf = nullptr;
    if (posix_memalign(&buf, IO_ALIGN, size) != 0)
    {
        perror("posix_memalign");
        return 1;
    }
    char *buffer = (char *)buf;
    memset(buffer, 0, size);
    int offset = 0;
    for (int i = 0; i < tuple_num; i++)
    {
        std::string str = "string" + std::to_string(status_distribution(gen));
        memcpy(buffer + offset, &strsize, sizeof(u16));
        memcpy(buffer + offset + 2, (str + str + str + str).data(), strsize);
        offset += strsize + 2;
    }

    if (!writeCF(filename, (char *)buffer, size))
    {
        return 1;
    }

    free(buffer);

    return 0;
}

const unsigned char **read_data(size_t count, long size, const char *filename)
{
    void *new_buf = nullptr;
    if (posix_memalign(&new_buf, IO_ALIGN, size) != 0)
    {
        perror("posix_memalign");
        return nullptr;
    }
    char *new_buffer = (char *)new_buf;
    int read = readCF(filename, new_buffer, size);
    if (!read)
    {
        throw std::runtime_error("failed to read file");
        return nullptr;
    }

    const unsigned char **strings = (const unsigned char **)malloc(count * sizeof(unsigned char *));

    auto offset = 0;
    for (size_t i = 0; i < count; i++)
    {
        u16 len = *(u16 *)(new_buffer + offset);
        offset += 2;
        char *key = new_buffer + offset;
        offset += len;
        strings[i] = (const unsigned char *)key;
    }

    for (size_t i = 0; i < 5; i++)
    {
        printf("data %s\n", strings[i]);
    }

    return strings;
}

static inline u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return u64(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

int test_fsst()
{
    // const unsigned char *str1 = (const unsigned char *)"tumcwitumvldb";
    // const unsigned char *str2 = (const unsigned char *)"another another another another another string to compress is key for compressiong another strings";
    // const unsigned char *str3 = (const unsigned char *)"yet another another another";

    // const unsigned char *strings[] = {str1, str2, str3}; // Array of pointers
    // const size_t lenIn[] = {strlen((const char *)str1), strlen((const char *)str2), strlen((const char *)str3)};

    constexpr long tuple_num = 1'000'000;
    constexpr size_t count = tuple_num;
    const u16 strsize = (u16)sizeof("string1") * 4 - 1;
    long size = align_up(tuple_num * (strsize + 2), IO_ALIGN);
    const char *filename = "resources/some.bin";
    std::cout << filename << size << strsize << tuple_num << std::endl;

    fill_data(size, tuple_num, strsize, filename);

    auto strings = read_data(count, size, filename);

    printf("Number of items %zu\n: ", count);

    size_t *lenIn = new size_t[count];
    std::fill(lenIn, lenIn + tuple_num, strsize);

    size_t total_size = size; // Input + some extra space
    unsigned char *output = (unsigned char *)malloc(total_size);

    size_t *lenOut = new size_t[count];                  // Will store compressed lengths for each string
    unsigned char **strOut = new unsigned char *[count]; // Will store pointers to compressed strings

    u64 t0 = now_ns();

    fsst_encoder_t *encoder = fsst_create(count, lenIn, strings, 0);

    u64 t1 = now_ns();

    // Compress all strings in batch
    fsst_compress(
        encoder,    // encoder
        count,      // nlines (number of strings)
        lenIn,      // input lengths array
        strings,    // input strings array
        total_size, // size of output buffer
        output,     // output buffer
        lenOut,     // output: compressed lengths for each string
        strOut      // output: pointers to each compressed string in output buffer
    );

    u64 t2 = now_ns();

    printf("fsst_create:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("fsst_compress: %.3f ms\n", (t2 - t1) / 1e6);
    printf("total fsst:    %.3f ms\n", (t2 - t0) / 1e6);

    // test_print_fsst_compression_results(compressed_total, lenIn, strings, lenOut, count);
    //  test_print_fsst_decompression_results(encoder, strOut, lenIn, strings, lenOut, count);

    return 0;
}

unsigned char **generate_str(size_t count, u32 *lens)
{
    const char *items[] = {"string1", "string2", "string3", "string4"};
    unsigned char **strings = (unsigned char **)malloc(count * sizeof(unsigned char *));

    for (size_t i = 0; i < count; i++)
    {
        int idx = rand() % 4;
        size_t len = strlen(items[idx]);
        strings[i] = (unsigned char *)malloc(len + 1);
        memcpy(strings[i], items[idx], len + 1);
        lens[i] = len;
    }

    for (size_t i = 0; i < 5; i++)
    {
        std::cout << strings[i] << std::endl;
    }

    return strings;
}

#include <bitset>
void test_bitpack()
{
    constexpr long tuple_num = align_up(2048, 256);
    auto data = (u32 *)malloc(tuple_num * sizeof(u32));
    auto n = 12;
    for (int i = 0; i < tuple_num; i++)
    {
        data[i] = i % (3);
    }
    auto out = (u32 *)malloc(tuple_num * sizeof(u32));
    u64 t0 = now_ns();
    BitPackEncoder::simd_encode((u64 *)out, data, tuple_num, n);

    std::cout << "----------class 1--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 0, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 1, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 2, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 3, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 4, n) << std::endl;

    std::cout << "----------class 2--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 252, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 253, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 254, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 255, n) << std::endl;

    std::cout << "----------class 3--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 0, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 1, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 2, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 3, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 4, n) << std::endl;

    std::cout << "----------class 4--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 252, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 253, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 254, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 256 + 255, n) << std::endl;

    std::cout << "----------class 5--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 80, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 81, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 82, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 83, n) << std::endl;

    std::cout << "----------class 5--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 95, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 96, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 97, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 98, n) << std::endl;

    std::cout << "----------class 6--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 195, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 196, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 197, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::simd_decode_single(out, 198, n) << std::endl;

    u64 t1 = now_ns();
    BitPackEncoder::simd_decode(out, (u64 *)data, tuple_num, 8);
    u64 t2 = now_ns();

    // for (int i = 0; i < 64; i++)
    // {
    //     std::cout << std::bitset<32>(out[i]) << std::endl;
    // }
    // std::cout << std::endl;

    printf("bitpack_encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("bitpack_decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("bitpack_total:   %.3f ms\n", (t2 - t0) / 1e6);
}

std::mt19937 rng(123456);
std::uniform_int_distribution<u32> disti(5, 1'000'000'000);

void test_fastpfor()
{
    constexpr long tuple_num = align_up(2048, 256);
    auto data = (u32 *)malloc(tuple_num * sizeof(u32));

    for (int i = 0; i < tuple_num; i++)
    {
        if (i % 150 == 0)
            data[i] = disti(rng);
        else
            data[i] = i % (4);
    }
    auto coded = (u32 *)malloc(tuple_num * sizeof(u32));

    auto encoder = FastPForEncoder();

    auto decoded = (u32 *)malloc(tuple_num * sizeof(u32));

    u64 t0 = now_ns();
    encoder.encode(coded, data, tuple_num);
    u64 t1 = now_ns();
    encoder.decode(decoded, coded, tuple_num);
    u64 t2 = now_ns();

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[i] << "-";
    }
    std::cout << std::endl;

    for (int i = 150; i < 150 + 20; i++)
    {
        std::cout << decoded[i] << "-";
    }
    std::cout << std::endl;

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[tuple_num - i - 1] << "-";
    }
    std::cout << std::endl;

    printf("bitpack_encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("bitpack_decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("bitpack_total:   %.3f ms\n", (t2 - t0) / 1e6);
}

void test_scalarbitpack()
{
    constexpr long tuple_num = align_up(2048, 256);
    auto data = (u32 *)malloc(tuple_num * sizeof(u32));

    for (int i = 0; i < tuple_num; i++)
    {
        data[i] = i % (4);
    }
    auto coded = (u64 *)malloc(tuple_num * sizeof(u64));

    auto decoded = (u32 *)malloc(tuple_num * sizeof(u32));

    u64 t0 = now_ns();
    BitPackEncoder::scalar_encode(coded, data, tuple_num, 2);
    u64 t1 = now_ns();
    BitPackEncoder::scalar_decode(decoded, coded, tuple_num, 2);
    u64 t2 = now_ns();

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[i] << "-";
    }
    std::cout << std::endl;

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[tuple_num - i - 1] << "-";
    }
    std::cout << std::endl;

    printf("bitpack_encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("bitpack_decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("bitpack_total:   %.3f ms\n", (t2 - t0) / 1e6);
}

void test_simdencode()
{
    constexpr long tuple_num = align_up(2048, 256);
    auto data = (u32 *)malloc(tuple_num * sizeof(u32));

    for (int i = 0; i < tuple_num; i++)
    {
        data[i] = i % (4);
    }
    auto coded = (u32 *)malloc(tuple_num * sizeof(u32));

    auto decoded = (u32 *)malloc(tuple_num * sizeof(u32));

    u64 t0 = now_ns();
    BitPackEncoder::simd_encode(coded, data, tuple_num, 2);
    u64 t1 = now_ns();
    BitPackEncoder::simd_decode(decoded, coded, tuple_num, 2);
    u64 t2 = now_ns();

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[i] << "-";
    }
    std::cout << std::endl;

    for (int i = 0; i < 20; i++)
    {
        std::cout << decoded[tuple_num - i - 1] << "-";
    }
    std::cout << std::endl;

    printf("bitpack_encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("bitpack_decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("bitpack_total:   %.3f ms\n", (t2 - t0) / 1e6);
}

void generate_strings(const u8 **&strings, u64 *&lens)
{
    // Allocate arrays for 4 strings (SIMD processes 4 at a time)
    static std::vector<std::vector<u8>> string_data;
    static const u8 *string_ptrs[4];
    static u64 string_lens[4];

    // Generate test strings
    string_data = {
        {'h', 'e', 'l', 'l', 'o'},
        {'w', 'o', 'r', 'l', 'd'},
        {'t', 'e', 's', 't'},
        {'d', 'a', 't', 'a'}};

    // Set up pointers and lengths
    for (int i = 0; i < 4; i++)
    {
        string_ptrs[i] = string_data[i].data();
        string_lens[i] = string_data[i].size();
    }

    strings = string_ptrs;
    lens = string_lens;
}

// #include <iomanip>

// void test_dict_hash()
// {
//     auto out = (__m256i *)malloc(sizeof(__m256i));
//     std::vector<u64> lens(8, 24);

//     const char *filename = "resources/some.bin";
//     // generate_strings(strings, lens);
//     fill_data(2048, 20, 24, filename);

//     auto strings = read_data(8, 2048, filename);

//     DictionaryEncoder::hash_fnv1a_simd(strings, lens.data(), out);

//     auto results = (u64 *)out;

//     std::cout << "Hash results:" << std::endl;
//     for (int i = 0; i < 4; i++)
//     {
//         std::cout << "String " << i << " (len=" << lens[i] << "): 0x"
//                   << std::dec << std::setw(16) << std::setfill('0')
//                   << results[i] << std::dec << std::endl;

//         std::cout << "Other string " << DictionaryEncoder::hash_fnv1a(strings[i], lens[i]) << std::endl;
//     }
// }

// void benchmark_simd_hash()
// {
//     const char *filename = "resources/some.bin";
//     // generate_strings(strings, lens);
//     auto tuple_num = 10'000;
//     auto str_size = 24;
//     auto size = align_up(tuple_num * (str_size + 2), IO_ALIGN);

//     fill_data(size, tuple_num, str_size, filename);

//     std::vector<u64> lens(tuple_num, str_size);
//     auto strings = read_data(tuple_num, size, filename);

//     u64 t0 = now_ns();
//     auto out = (__m256i *)malloc(sizeof(__m256i) * (tuple_num / 4));
//     for (int i = 0; i < tuple_num / 4; i++)
//     {
//         DictionaryEncoder::hash_fnv1a_simd(strings + 4 * i, lens.data() + 4 * i, out);
//         out++;
//     }

//     // auto out = (u64 *)malloc(sizeof(u64) * 200 * 4);
//     // for (int i = 0; i < tuple_num / 4; i++)
//     // {
//     //     auto val = DictionaryEncoder::hash_fnv1a(strings[i], lens.data()[i]);
//     //     out[i] = val;
//     //     out++;
//     // }

//     u64 t1 = now_ns();

//     printf("time:   %.3f ms\n", (t1 - t0) / 1e6);
// }

void test_bitpacking_scalar()
{
    BitPackEncoder encoder;
    std::vector<u32> input;
    std::vector<u64> encoded;
    std::vector<u32> decoded;

    std::vector<u32> values;
    u32 nu = 100'000;
    for (u32 i = 0; i < nu; i++)
    {
        values.push_back(i);
    }

    u32 usedBits = 17;

    input = values;

    // Calculate maximum encoded size
    size_t encodedSize = ((values.size() * usedBits + 63) / 64) + 1;
    encoded.resize(encodedSize, 0);
    decoded.resize(values.size(), 0);

    u64 t0 = now_ns();
    encoder.scalar_encode(encoded.data(), input.data(), input.size(), usedBits);
    u64 t1 = now_ns();
    u32 *decEnd = encoder.scalar_decode(decoded.data(), encoded.data(), input.size(), usedBits);
    u64 t2 = now_ns();

    (void)decEnd; // To remove warnings in release mode

    printf("encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("total:    %.3f ms\n", (t2 - t0) / 1e6);

    // Verify
    u32 mask = (1ULL << usedBits) - 1;

    for (size_t i = 0; i < values.size(); i++)
    {
        assert(decoded[i] == (values[i] & mask)); // TODO fix this
    }

    assert(decEnd == decoded.data() + values.size());

    (void)mask; // To remove warnings in release mode
}

std::string generateRandomString(size_t length)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; i++)
    {
        result += charset[dist(rng)];
    }
    return result;
}

u8 *makeString(const char *str)
{
    u32 len = strlen(str);
    u8 *result = new u8[len + 1];
    memcpy(result, str, len + 1);
    // allocatedStrings.push_back(result);
    return result;
}

u8 *makeString(const std::string &str)
{
    return makeString(str.c_str());
}

void test_huge_dict()
{
    const int COUNT = 5'000'000;
    // std::vector<u32> encoded(COUNT * 100); // Large buffer for unique strings
    std::vector<u8 *> inStrings(COUNT);
    std::vector<u32> inLengths(COUNT);

    for (int i = 0; i < COUNT; i++)
    {
        std::string str = "str_" + status_distribution(gen); // std::to_string(i) + "_" + generateRandomString(20);
        inStrings[i] = makeString(str);
        inLengths[i] = str.length();
    }

    u32 totalStrLen = 0;
    for (int i = 0; i < COUNT; i++)
    {
        totalStrLen += inLengths[i];
    }

    std::vector<u8 *> outStrings(COUNT);
    std::vector<u32> outLengths(COUNT);

    auto encoded = DictionaryStringEncodedRes{
        .codes = (u32 *)malloc(COUNT * sizeof(u32)),
        .indexes = (u32 *)malloc((COUNT + 1) * sizeof(u32)),
        .strings = (u8 *)malloc(totalStrLen),
    };

    u64 t0 = now_ns();
    DictionaryStringEncoder::encode(&encoded, inStrings.data(), inLengths.data(), COUNT);
    u64 t1 = now_ns();
    DictionaryStringEncoder::decode(outStrings.data(), outLengths.data(), &encoded, COUNT);
    u64 t2 = now_ns();

    for (int i = 0; i < COUNT; i++)
    {
        assert(outLengths[i] == inLengths[i]);
        assert(memcmp(outStrings[i], inStrings[i], inLengths[i]) == 0);
    }

    printf("encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("total:    %.3f ms\n", (t2 - t0) / 1e6);
}

void test_rle_encoder()
{
    RleEncoder<u32> encoder;

    // Test 5: Large runs
    {
        std::mt19937 rng(42);
        std::vector<u32> input;
        u32 currentValue = 0;
        for (int i = 0; i < 1'000'000;)
        {
            // Random run length between 1 and 100
            u32 runLength = 1 + (rng() % 100);

            // Don't exceed total size
            runLength = std::min(runLength, 1'000'000u - i);

            // Add the run
            for (u32 j = 0; j < runLength; j++)
            {
                input.push_back(currentValue);
            }

            i += runLength;
            currentValue = (currentValue + 1) % 10;
        }

        std::vector<u32> encodedData(input.size());
        std::vector<u16> encodedLen(input.size());

        auto encoded = RleEncodedRes<u32>{
            .values = encodedData.data(),
            .counts = encodedLen.data(),
            .size = 0,
        };

        std::vector<u32> decoded(input.size());

        u64 t0 = now_ns();
        encoder.encode(&encoded, input.data(), input.size());
        u64 t1 = now_ns();
        encoder.decode(decoded.data(), &encoded);
        u64 t2 = now_ns();

        for (size_t i = 0; i < input.size(); i++)
        {
            assert(decoded[i] == input[i]);
        }

        std::cout << "Test 5 (large runs): PASSED\n";

        printf("encode:   %.3f ms\n", (t1 - t0) / 1e6);
        printf("decode:   %.3f ms\n", (t2 - t1) / 1e6);
        printf("total:    %.3f ms\n", (t2 - t0) / 1e6);
    }

    std::cout << "\nAll RLE tests PASSED!\n";
}

#include <sched.h>
#include <unistd.h>

int reserveCpuCore()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset); // Pin to core 0 (change number for different core)

    pid_t pid = getpid();
    int result = sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);

    if (result == -1)
    {
        std::cerr << "Failed to set CPU affinity" << std::endl;
        return 1;
    }

    std::cout << "Process pinned to CPU core 0" << std::endl;

    return 0;
}

void test_huge_dict_values()
{
    const int COUNT = 5'000'000;
    // std::vector<u16> encoded(COUNT * 100); // Large buffer for unique strings
    std::vector<u32> in(COUNT);

    for (int i = 0; i < COUNT; i++)
    {
        in[i] = i + 1; // status_distribution(gen);
    }

    std::vector<u32> out(COUNT);

    auto encoded = DictionaryValueEncodedRes<u32>{
        .codes = (u32 *)malloc(COUNT * sizeof(u32)),
        .values = (u32 *)malloc(COUNT * sizeof(u32)),
        .valCount = 0};

    u64 t0 = now_ns();
    DictionaryValueEncoder<u32>::encode(&encoded, in.data(), COUNT);
    u64 t1 = now_ns();
    DictionaryValueEncoder<u32>::decode(out.data(), &encoded, COUNT);
    u64 t2 = now_ns();

    for (int i = 0; i < COUNT; i++)
    {
        assert(out[i] == in[i]);
    }

    printf("encode:   %.3f ms\n", (t1 - t0) / 1e6);
    printf("decode:   %.3f ms\n", (t2 - t1) / 1e6);
    printf("total:    %.3f ms\n", (t2 - t0) / 1e6);

    std::cout << "works" << std::endl;
}

template <typename ValueType>
void roundtrip(ValueType *input, u32 count, u32 expectedDistinct)
{
    DictionaryValueEncoder<ValueType> encoded{};
    ValueType *decoded = nullptr;

    encoded.codes = (ValueType *)malloc(count * sizeof(ValueType));
    encoded.values = (ValueType *)malloc(count * sizeof(ValueType));
    encoded.valCount = 0;

    DictionaryValueEncoder<ValueType>::encode(&encoded, input, count);
    assert(encoded.valCount == expectedDistinct);

    decoded = (ValueType *)malloc(count * sizeof(ValueType));
    DictionaryValueEncoder<ValueType>::decode(decoded, &encoded, count);

    for (u32 i = 0; i < count; i++)
    {
        assert(decoded[i] == input[i]);
    }
}

void test_oneval_encoder()
{
    // Test 5: Large runs
    {
        std::vector<u32> input;
        u32 currentValue = 5;
        u32 COUNT = 1'000'000;
        for (u32 i = 0; i < COUNT; i++)
        {
            input.push_back(5);
        }

        std::vector<u32> encoded(1);

        std::vector<u32> decoded(input.size() + 32);

        u64 t0 = now_ns();
        OneValEncoder<u32>::encode(encoded.data(), input.data());
        assert(encoded[0] == currentValue);
        u64 t1 = now_ns();
        OneValEncoder<u32>::decode(decoded.data(), encoded[0], COUNT);
        u64 t2 = now_ns();

        for (size_t i = 0; i < input.size(); i++)
        {
            assert(decoded[i] == currentValue);
        }

        std::cout << "Test 5 (large runs): PASSED\n";

        printf("encode:   %.3f ms\n", (t1 - t0) / 1e6);
        printf("decode:   %.3f ms\n", (t2 - t1) / 1e6);
        printf("total:    %.3f ms\n", (t2 - t0) / 1e6);

        (void)currentValue;
    }

    std::cout << "\nAll RLE tests PASSED!\n";
}

void test_append_valtyp_map()
{
    u32 COUNT = 10;
    AppendOnlyHMap<u32> map(COUNT);

    std::vector<u32> vec(COUNT);
    for (u32 i = 0; i < COUNT; i++)
    {
        vec[i] = i + 1;
    }

    for (u32 i = 0; i < COUNT; i++)
    {
        assert(vec[i] == map.simd_get_insert(i, i + 1));
    }

    std::cout << "okkk" << std::endl;
}

int main()
{
    test_huge_dict_values();
    // test_huge_dict();
    // test_oneval_encoder();

    // u8 data[256];
    // for (int i = 0; i < 256; i++)
    //     data[i] = (u8)i;
    // roundtrip(data, 256, 256);

    // test_append_valtyp_map();

    return 0;
}