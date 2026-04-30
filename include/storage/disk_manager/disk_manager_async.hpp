#pragma once

#include "storage/storage_common.hpp"
#include "storage/disk_manager/fd_cache.hpp"

#include <liburing.h>
#include <cstdint>
#include <functional>
#include <memory>

#define IOURING_QUEUE_SIZE 1024
/**
 * this is a hack
 */
#define MAX_PATH_LEN 256u
#define MAX_PATH_LEN_2 128u

namespace db7::storage
{
    class DiskManagerAsync
    {
    private:
        char base_dir_[MAX_PATH_LEN_2]; /* directory that holds table files */
        std::unique_ptr<FdCache> cache_;

        struct io_uring ring_;
        using CompletionCb = std::function<void(void *user_data, ssize_t result)>;
        CompletionCb completion_cb_;

        void BuildPath(table_id tid, char *buf, u32 len);
        void LoadExistingTables();

    public:
        explicit DiskManagerAsync(const char *base_dir, u32 queue_depth = IOURING_QUEUE_SIZE);
        ~DiskManagerAsync();
        void SetCompletionCallback(CompletionCb cb) { completion_cb_ = std::move(cb); }

        bool SubmitRead(table_id pid, void *buf, u32 len, off_t offset, void *user_data);
        bool SubmitWrite(table_id pid, const void *buf, u32 len, off_t offset, void *user_data);
        int ReapCompletions(u32 max_completions = IOURING_QUEUE_SIZE);
        int Submit();

        bool CreateTable(table_id tbl_id);
        bool OpenFile(table_id tbl_id);
        bool DropTable(table_id tbl_id);
        bool ExistsTable(table_id tbl_id);
        bool TruncateFile(table_id tbl_id, u64 pages_num);
        u32 PageCount(table_id tbl_id);
    };
}