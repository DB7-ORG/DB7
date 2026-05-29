#include <cstdio>

int TESTS_LAYOUT_LEAF();
int TESTS_LAYOUT_INTER();
int TESTS_KEY_NORM_ENCODER();

int main()
{
    TESTS_KEY_NORM_ENCODER();
    TESTS_LAYOUT_LEAF();
    TESTS_LAYOUT_INTER();
    printf("\nAll tests passed!\n");
    return 0;
}