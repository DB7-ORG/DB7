#pragma once

#include "storage/storage_common.hpp"

#include <liburing.h>
#include <cstdint>
#include <functional>

#define IOURING_QUEUE_SIZE 512

namespace db7::storage
{
    // struct IoRequest
    // {
    //     enum Op
    //     {
    //         READ,
    //         WRITE
    //     };
    //     Op op;
    //     int fd;
    //     void *buf;
    //     u32 len;
    //     off_t offset;
    //     void *user_data; // callback context
    // };

    class DiskManagerAsync
    {
    private:
        struct io_uring ring_;
        using CompletionCb = std::function<void(void *user_data, ssize_t result)>;
        CompletionCb completion_cb_;

    public:
        explicit DiskManagerAsync(u32 queue_depth = IOURING_QUEUE_SIZE);
        ~DiskManagerAsync();

        // submit a single read/write, returns false if queue full
        bool SubmitRead(int fd, void *buf, u32 len, off_t offset, void *user_data);
        bool SubmitWrite(int fd, const void *buf, u32 len, off_t offset, void *user_data);

        // block until at least 1 completion, returns number reaped
        int ReapCompletions(u32 max_completions = IOURING_QUEUE_SIZE);

        // flush pending submissions to kernel
        int Submit();

        // callback for completions
        void SetCompletionCallback(CompletionCb cb) { completion_cb_ = std::move(cb); }
    };
}