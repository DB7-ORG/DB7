#include "shared/key_encode_util.hpp"
#include <cstdio>
#include <cassert>
#include <cstring>

namespace db7::shared
{
    namespace
    {
        void TestUnsignedEncoding()
        {
            byte buf1[4], buf2[4];

            KeyNormEncoder::Encode(buf1, u32(1), false, false, false);
            KeyNormEncoder::Encode(buf2, u32(2), false, false, false);

            assert(std::memcmp(buf1, buf2, 4) < 0);

            printf("TestUnsignedEncoding PASSED\n");
        }

        void TestSignedEncoding()
        {
            byte buf1[4], buf2[4], buf3[4];

            KeyNormEncoder::Encode(buf1, i32(-1), false, false, false);
            KeyNormEncoder::Encode(buf2, i32(0), false, false, false);
            KeyNormEncoder::Encode(buf3, i32(1), false, false, false);

            // -1 < 0 < 1 must hold under memcmp
            assert(std::memcmp(buf1, buf2, 4) < 0);
            assert(std::memcmp(buf2, buf3, 4) < 0);
            assert(std::memcmp(buf1, buf3, 4) < 0);

            printf("TestSignedEncoding PASSED\n");
        }

        void TestDoubleEncoding()
        {
            byte buf1[8], buf2[8], buf3[8], buf4[8];

            KeyNormEncoder::Encode(buf1, double(-1.5), false, false, false);
            KeyNormEncoder::Encode(buf2, double(-0.5), false, false, false);
            KeyNormEncoder::Encode(buf3, double(0.0), false, false, false);
            KeyNormEncoder::Encode(buf4, double(1.5), false, false, false);

            assert(std::memcmp(buf1, buf2, 8) < 0);
            assert(std::memcmp(buf2, buf3, 8) < 0);
            assert(std::memcmp(buf3, buf4, 8) < 0);

            printf("TestDoubleEncoding PASSED\n");
        }

        void TestStringCaseSensitive()
        {
            byte buf1[8], buf2[8];

            const char *s1 = "apple";
            const char *s2 = "banana";

            KeyNormEncoder::Encode(buf1, std::span<const byte>(reinterpret_cast<const byte *>(s1), 5), false, false, true);
            KeyNormEncoder::Encode(buf2, std::span<const byte>(reinterpret_cast<const byte *>(s2), 6), false, false, true);

            assert(std::memcmp(buf1, buf2, 5) < 0);

            printf("TestStringCaseSensitive PASSED\n");
        }

        void TestStringCaseInsensitive()
        {
            byte buf1[8], buf2[8];

            const char *s1 = "APPLE";
            const char *s2 = "apple";

            KeyNormEncoder::Encode(buf1, std::span<const byte>(reinterpret_cast<const byte *>(s1), 5), false, false, false);
            KeyNormEncoder::Encode(buf2, std::span<const byte>(reinterpret_cast<const byte *>(s2), 5), false, false, false);

            // case insensitive — should be equal
            assert(std::memcmp(buf1, buf2, 6) == 0);

            printf("TestStringCaseInsensitive PASSED\n");
        }

        void TestNullable()
        {
            byte buf1[8], buf2[8];

            // null sorts before non-null
            KeyNormEncoder::Encode(buf1, i32(0), true, true, false);  // null
            KeyNormEncoder::Encode(buf2, i32(0), false, true, false); // not null

            assert(std::memcmp(buf1, buf2, 1) < 0); // 0x00 < 0x01

            printf("TestNullable PASSED\n");
        }

        void TestNullableEarlyReturn()
        {
            byte buf1[8];
            std::memset(buf1, 0xFF, sizeof(buf1));

            u32 size = KeyNormEncoder::Encode(buf1, i32(42), true, true, false);

            // only null byte written, rest untouched
            assert(size == 1);
            assert(buf1[0] == 0x00);
            assert(buf1[1] == 0xFF); // untouched

            printf("TestNullableEarlyReturn PASSED\n");
        }
    }
}

int TESTS_KEY_NORM_ENCODER()
{
    db7::shared::TestUnsignedEncoding();
    db7::shared::TestSignedEncoding();
    db7::shared::TestDoubleEncoding();
    db7::shared::TestStringCaseSensitive();
    db7::shared::TestStringCaseInsensitive();
    db7::shared::TestNullable();
    db7::shared::TestNullableEarlyReturn();

    printf("\nAll KeyNormEncoder tests passed!\n");
    return 0;
}