#include <iostream>
#include "storage/disk_manager.h"
#include "storage/compressions/compression.h"
#include <random>
#include <string.h>
#include "storage/compressions/fsst.h"
#include <time.h>

std::mt19937 gen(42);
std::uniform_int_distribution<> status_distribution(1, 8);

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
        memcpy(buffer + offset + 2, (str + str + str + str).c_str(), strsize);
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

void test_dict()
{
    constexpr long tuple_num = 1'000'000;
    constexpr size_t count = tuple_num;
    const u16 strsize = (u16)sizeof("string1") * 4 - 1;
    long size = align_up(tuple_num * (strsize + 2), IO_ALIGN);
    const char *filename = "resources/some.bin";
    std::cout << filename << size << strsize << tuple_num << std::endl;

    fill_data(size, tuple_num, strsize, filename);

    read_data(count, size, filename);

    auto strings = read_data(count, size, filename);

    printf("Number of items %zu\n: ", count);

    size_t *lenIn = new size_t[count];
    std::fill(lenIn, lenIn + tuple_num, strsize);

    u32 *encoded = (u32 *)malloc(count * sizeof(u32));

    u64 t0 = now_ns();

    DictionaryEncoder::encode(count, (unsigned char **)strings, lenIn, encoded);

    u64 t1 = now_ns();

    printf("dict_encode:   %.3f ms\n", (t1 - t0) / 1e6);
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
    BitPackEncoder::encode((u64 *)out, data, tuple_num, n);

    std::cout << "----------class 1--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 0, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 1, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 2, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 3, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 4, n) << std::endl;

    std::cout << "----------class 2--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 252, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 253, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 254, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 255, n) << std::endl;

    std::cout << "----------class 3--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 0, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 1, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 2, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 3, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 4, n) << std::endl;

    std::cout << "----------class 4--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 252, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 253, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 254, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 256 + 255, n) << std::endl;

    std::cout << "----------class 5--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 80, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 81, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 82, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 83, n) << std::endl;

    std::cout << "----------class 5--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 95, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 96, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 97, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 98, n) << std::endl;

    std::cout << "----------class 6--------------" << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 195, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 196, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 197, n) << std::endl;
    std::cout << "value is " << BitPackEncoder::decode_single(out, 198, n) << std::endl;

    u64 t1 = now_ns();
    // BitPackEncoder::decode(out, (u64 *)data, tuple_num, 8);
    // u64 t2 = now_ns();

    // for (int i = 0; i < 64; i++)
    // {
    //     std::cout << std::bitset<32>(out[i]) << std::endl;
    // }
    // std::cout << std::endl;

    // printf("bitpack_encode:   %.3f ms\n", (t1 - t0) / 1e6);
    // printf("bitpack_decode:   %.3f ms\n", (t2 - t1) / 1e6);
    // printf("bitpack_total:   %.3f ms\n", (t2 - t0) / 1e6);
}

int main()
{
    test_bitpack();
    return 0;
}