#include "storage/disk_manager/disk_manager_async.hpp"

#include <stdexcept>
#include <cstring>

namespace db7::storage
{
    DiskManagerAsync::DiskManagerAsync(u32 queue_depth)
    {
        int ret = io_uring_queue_init(queue_depth, &ring_, 0);
        if (ret < 0)
            throw std::runtime_error("io_uring_queue_init failed");
    }

    DiskManagerAsync::~DiskManagerAsync()
    {
        io_uring_queue_exit(&ring_);
    }

    /**
     * sqe - Submission Queue Entry
     */
    bool DiskManagerAsync::SubmitRead(int fd, void *buf, u32 len, off_t offset, void *user_data)
    {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe)
            return false;

        io_uring_prep_read(sqe, fd, buf, len, offset);
        io_uring_sqe_set_data(sqe, user_data);
        return true;
    }

    bool DiskManagerAsync::SubmitWrite(int fd, const void *buf, u32 len, off_t offset, void *user_data)
    {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe)
            return false;

        io_uring_prep_write(sqe, fd, buf, len, offset);
        io_uring_sqe_set_data(sqe, user_data);
        return true;
    }

    int DiskManagerAsync::Submit()
    {
        return io_uring_submit(&ring_);
    }

    int DiskManagerAsync::ReapCompletions(u32 max_completions)
    {
        struct io_uring_cqe *cqe;
        int reaped = 0;

        // block for at least one
        int ret = io_uring_wait_cqe(&ring_, &cqe);
        if (ret < 0)
            return ret;

        do
        {
            void *data = io_uring_cqe_get_data(cqe);
            ssize_t result = cqe->res;

            if (completion_cb_)
                completion_cb_(data, result);

            io_uring_cqe_seen(&ring_, cqe);
            reaped++;

            if ((u32)reaped >= max_completions)
                break;
        } while (io_uring_peek_cqe(&ring_, &cqe) == 0);

        return reaped;
    }
}