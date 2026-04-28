#include "storage/disk_manager/disk_manager_async.hpp"

#include <stdexcept>
#include <cstring>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#define DIRECT_ALIGN 4096
#define INIT_FREE_PAGES 3

namespace db7::storage
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    void DiskManagerAsync::BuildPath(table_id tid, char *buf, u32 len)
    {
        int n = snprintf(buf, len, "%s/table_%u.db", base_dir_, (unsigned)tid);
        (void)n;
        DB7_ASSERT(n > 0 && (u32)n < len, "Path buffer too small");
    }
#pragma GCC diagnostic pop

    void DiskManagerAsync::LoadExistingTables()
    {
        DIR *dir = opendir(base_dir_);
        if (!dir)
            return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type != DT_REG)
                continue;

            table_id tbl_id;
            if (sscanf(entry->d_name, "table_%u", &tbl_id) != 1)
                continue;

            char path[MAX_PATH_LEN];
            BuildPath(tbl_id, path, sizeof(path));

            int fd = open(path, O_RDWR | O_DIRECT);
            if (fd < 0)
                continue;

            struct stat st;
            fstat(fd, &st);

            FdCacheEntry hdr;
            hdr.fd = fd;
            hdr.page_count = st.st_size / PAGE_SIZE;

            cache_->Set(tbl_id, hdr);
        }

        closedir(dir);
    }

    DiskManagerAsync::DiskManagerAsync(const char *base_dir, u32 queue_depth)
        : cache_(std::make_unique<FdCache>())
    {
        int ret = io_uring_queue_init(queue_depth, &ring_, 0);
        if (ret < 0)
            throw std::runtime_error("io_uring_queue_init failed");

        strncpy(base_dir_, base_dir, MAX_PATH_LEN_2 - 1);

        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            cache_->Invalidate(i);
        }

        mkdir(base_dir_, 0755);

        LoadExistingTables();
    }

    DiskManagerAsync::~DiskManagerAsync()
    {
        io_uring_queue_exit(&ring_);

        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            int fd = cache_->Get(i).fd;
            if (fd >= 0)
            {
                close(fd);
            }
        }
    }

    /**
     * sqe - Submission Queue Entry
     */
    bool DiskManagerAsync::SubmitRead(table_id tid, void *buf, u32 len, off_t offset, void *user_data)
    {
        auto hdr = cache_->Get(tid);
        DB7_ASSERT(hdr.fd >= 0, "No valid file descriptor");

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe)
            return false;

        io_uring_prep_read(sqe, hdr.fd, buf, len, offset);
        io_uring_sqe_set_data(sqe, user_data);
        return true;
    }

    bool DiskManagerAsync::SubmitWrite(table_id tid, const void *buf, u32 len, off_t offset, void *user_data)
    {
        auto hdr = cache_->Get(tid);
        DB7_ASSERT(hdr.fd >= 0, "No valid file descriptor");

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe)
            return false;

        io_uring_prep_write(sqe, hdr.fd, buf, len, offset);
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

        while (io_uring_peek_cqe(&ring_, &cqe) == 0)
        {
            void *data = io_uring_cqe_get_data(cqe);
            ssize_t result = cqe->res;

            if (completion_cb_) // always true
                completion_cb_(data, result);

            io_uring_cqe_seen(&ring_, cqe);
            reaped++;

            if ((u32)reaped >= max_completions)
                break;
        }

        return reaped;
    }

    bool DiskManagerAsync::CreateTable(table_id tbl_id)
    {
        if (cache_->Get(tbl_id).fd != -1)
        {
            DB7_ASSERT(false, "File already exists");
            return false;
        }

        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));

        int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_DIRECT, 0644);
        if (fd < 0)
        {
            DB7_ASSERT(false, "File not found");
            return false;
        }

        FdCacheEntry entry(fd, INIT_FREE_PAGES);
        cache_->Set(tbl_id, entry);

        TruncateFile(tbl_id, INIT_FREE_PAGES);

        return true;
    }

    bool DiskManagerAsync::DropTable(table_id tbl_id)
    {
        auto hdr = cache_->Invalidate(tbl_id);
        DB7_ASSERT(hdr.fd >= 0, "Invalid file");

        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));

        close(hdr.fd);
        unlink(path);

        return true;
    }

    bool DiskManagerAsync::ExistsTable(table_id tbl_id)
    {
        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));
        return access(path, F_OK) == 0;
    }

    bool DiskManagerAsync::TruncateFile(table_id tbl_id, u32 pages_num)
    {
        FdCacheEntry hdr = cache_->Get(tbl_id);
        DB7_ASSERT(hdr.fd >= 0, "File not found");

        int n = ftruncate(hdr.fd, (pages_num)*PAGE_SIZE);
        (void)n;
        DB7_ASSERT(n == 0, "Truncate failed");
        hdr.page_count = pages_num;

        cache_->Set(tbl_id, hdr);

        return true;
    }

    u32 DiskManagerAsync::PageCount(table_id tbl_id)
    {
        FdCacheEntry hdr = cache_->Get(tbl_id);
        return hdr.page_count;
    }
}