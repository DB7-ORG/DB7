#include <iostream>
#include <random>
#include <cstdlib>
// #include <sys/stat.h>
// #include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#define GENERATE_FILE true

std::mt19937 gen(42);

constexpr std::size_t ROW_NUM = 1'000'000;
constexpr std::size_t GROUP_NUM = 5;

std::uniform_int_distribution<> status_distribution(1, 8);
std::uniform_int_distribution<> customer_distribution(1, 1'000'000);
std::normal_distribution<> money_distribution(5'000'000, 1'666'666);

struct RowGroupHeader
{
    // TODO some random data
};

struct Footer
{
    // TODO some random data
};

int counter = 0;
uint32_t generateId()
{
    counter++;
    return (uint32_t)counter;
}

uint8_t generateStatus()
{
    return (uint8_t)status_distribution(gen);
}

uint32_t generateCustomerId()
{
    return (uint32_t)customer_distribution(gen);
}

uint64_t generateMoney()
{
    return (uint64_t)money_distribution(gen);
}

int alignOffset(int offset, size_t alignment = 8)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

template <typename T>
int generate(char *data, int offset, T (*generatorFunc)())
{
    offset = alignOffset(offset, alignof(T));

    T *ptr = (T *)((char *)data + offset);
    for (size_t i = 0; i < (size_t)ROW_NUM; i++)
    {
        ptr[i] = generatorFunc();
    }

    if (!GENERATE_FILE)
    {
        auto buffer = (T *)(data + offset);
        for (size_t i = 0; i < ROW_NUM; i++)
        {
            std::cout << (long)buffer[i] << " ";
        }

        std::cout << std::endl;
    }

    return offset + ROW_NUM * sizeof(T);
}

size_t allocateRowGroupSize()
{
    size_t size = sizeof(RowGroupHeader);
    size = alignOffset(size, alignof(uint32_t)) + ROW_NUM * sizeof(uint32_t);
    size = alignOffset(size, alignof(uint8_t)) + ROW_NUM * sizeof(uint8_t);
    size = alignOffset(size, alignof(uint32_t)) + ROW_NUM * sizeof(uint32_t);
    size = alignOffset(size, alignof(uint64_t)) + ROW_NUM * sizeof(uint64_t);
    return size;
}

void appendToFile(int fd, char *data, size_t size)
{
    if (GENERATE_FILE)
    {
        ssize_t bytes_written = write(fd, data, size);
        if (bytes_written == -1)
        {
            perror("Error writing to file");
            close(fd);
            free(data);
            return;
        }

        std::cout << "Appended " << bytes_written << " bytes to file" << std::endl;
    }
}

int main()
{
    int fd = open("data.cf", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }

    size_t size = allocateRowGroupSize();

    auto data = (char *)malloc(size);

    for (int i = 0; i < (int)GROUP_NUM; i++)
    {
        int offset = generate(data, sizeof(RowGroupHeader), generateId);
        offset = generate(data, offset, generateStatus);
        offset = generate(data, offset, generateCustomerId);
        offset = generate(data, offset, generateMoney);
        appendToFile(fd, data, size);
    }

    // TODO write footer (Footer)

    close(fd);
    free(data);

    return 0;
}