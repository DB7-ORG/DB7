#include <iostream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include "../src/storage/compressions/compression.h"

// The generalized decoder
uint32_t simd_decode_single(const uint32_t *compressed, uint32_t idx, uint32_t bits)
{
    return BitPackEncoder::simd_decode_single(compressed, idx, bits);
}

// Simple scalar encoder for testing (matches the SIMD layout)
void encode_scalar(uint32_t *compressed, uint32_t *values, uint32_t nitems, uint32_t bits)
{
    BitPackEncoder::simd_encode(compressed, values, nitems, bits);
}

// Test sequential values
void test_sequential(uint32_t bits)
{
    uint32_t mask = (1U << bits) - 1;
    uint32_t values[256];
    uint32_t compressed[32 * 8]; // max size for 32 bits

    uint32_t nitems = 256;
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = i & mask;
    }

    encode_scalar(compressed, values, nitems, bits);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = simd_decode_single(compressed, i, bits);
        uint32_t expected = i & mask;
        if (decoded != expected)
        {
            std::cout << "FAIL test_sequential bits=" << bits << " idx=" << i
                      << " expected=" << expected << " got=" << decoded << std::endl;
            exit(1);
        }
    }

    std::cout << "PASS: sequential test for bits=" << bits << std::endl;
}

// Test all max values
void test_max_values(uint32_t bits)
{
    uint32_t mask = (1U << bits) - 1;
    uint32_t values[256];
    uint32_t compressed[32 * 8];

    uint32_t nitems = 256;
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = mask; // all bits set
    }

    encode_scalar(compressed, values, nitems, bits);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = simd_decode_single(compressed, i, bits);
        if (decoded != mask)
        {
            std::cout << "FAIL test_max_values bits=" << bits << " idx=" << i
                      << " expected=" << mask << " got=" << decoded << std::endl;
            exit(1);
        }
    }

    std::cout << "PASS: max values test for bits=" << bits << std::endl;
}

// Test alternating pattern
void test_alternating(uint32_t bits)
{
    uint32_t mask = (1U << bits) - 1;
    uint32_t values[256];
    uint32_t compressed[32 * 8];

    uint32_t nitems = 256;
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = (i % 2 == 0) ? 0 : mask;
    }

    encode_scalar(compressed, values, nitems, bits);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t expected = (i % 2 == 0) ? 0 : mask;
        uint32_t decoded = simd_decode_single(compressed, i, bits);
        if (decoded != expected)
        {
            std::cout << "FAIL test_alternating bits=" << bits << " idx=" << i
                      << " expected=" << expected << " got=" << decoded << std::endl;
            exit(1);
        }
    }

    std::cout << "PASS: alternating test for bits=" << bits << std::endl;
}

// Test spanning boundaries specifically
void test_spanning(uint32_t bits)
{
    uint32_t mask = (1U << bits) - 1;
    uint32_t values[256];
    uint32_t compressed[32 * 8];

    // Fill with unique values
    uint32_t nitems = 256;
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = (i * 7 + 3) & mask; // pseudo-random pattern
    }

    encode_scalar(compressed, values, nitems, bits);

    // Check specifically around word boundaries
    for (uint32_t lane = 0; lane < 32; lane++)
    {
        uint32_t bitpos = lane * bits;
        uint32_t shift = bitpos & 31;

        if (shift > 32 - bits)
        {
            // This lane spans a word boundary
            for (uint32_t pos = 0; pos < 8; pos++)
            {
                uint32_t idx = lane * 8 + pos;
                uint32_t decoded = simd_decode_single(compressed, idx, bits);
                uint32_t expected = values[idx];
                if (decoded != expected)
                {
                    std::cout << "FAIL  test_spanning bits=" << bits << " idx=" << idx
                              << " lane=" << lane << " shift=" << shift
                              << " expected=" << expected << " got=" << decoded << std::endl;
                    exit(1);
                }
            }
        }
    }

    std::cout << "PASS: spanning test for bits=" << bits << std::endl;
}

// Test multiple blocks
void test_multiple_blocks(uint32_t bits)
{
    uint32_t mask = (1U << bits) - 1;
    uint32_t values[512];
    uint32_t compressed[64 * 8]; // 2 blocks

    uint32_t nitems = 512;
    for (uint32_t i = 0; i < nitems; i++)
    {
        values[i] = i & mask;
    }

    // Encode two blocks
    encode_scalar(compressed, values, nitems, bits);

    for (uint32_t i = 0; i < nitems; i++)
    {
        uint32_t decoded = simd_decode_single(compressed, i, bits);
        uint32_t expected = i & mask;
        if (decoded != expected)
        {
            std::cout << "FAIL test_multiple_blocks bits=" << bits << " idx=" << i
                      << " expected=" << expected << " got=" << decoded << std::endl;
            exit(1);
        }
    }

    std::cout << "PASS: multiple blocks test for bits=" << bits << std::endl;
}

int main()
{
    std::cout << "Testing simd_decode_single for bits 1-31" << std::endl;
    std::cout << "===================================" << std::endl;

    for (uint32_t bits = 1; bits <= 31; bits++)
    {
        test_sequential(bits);
        test_max_values(bits);
        test_alternating(bits);
        test_spanning(bits);
        test_multiple_blocks(bits);
        std::cout << std::endl;
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}