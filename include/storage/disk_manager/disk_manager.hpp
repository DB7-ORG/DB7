#pragma once

#include "storage/storage_common.hpp"
#include "storage/disk_manager/fd_cache_entry.hpp"
#include "storage/disk_manager/table_file_header.hpp"

#define MAX_OPEN_FILES 64u
#define MAX_PATH_LEN 256u
#define MAX_PATH_LEN 256u
#define MAGIC_NUMBER 0xDB0101DBu

namespace db7::storage
{
    class DiskManager
    {
    private:
        char base_dir_[MAX_PATH_LEN]; /* directory that holds table files */

        /* --- fd cache -------------------------------------------------------- */
        FdCacheEntry cache_[MAX_OPEN_FILES];
        uint32_t cache_size_;     /* number of entries currently used */
        uint64_t access_counter_; /* monotonically increasing clock  */

        /* LRU list: head = most recent, tail = eviction candidate */
        FdCacheEntry *lru_head_;
        FdCacheEntry *lru_tail_;

        void BuildPath(table_id tid, char *buf, u32 len);
        void LruDetach(FdCacheEntry *e);
        void LruPushFront(FdCacheEntry *e);
        void LruTouch(FdCacheEntry *e);
        FdCacheEntry *CacheFind(table_id tid);
        void CacheEvict();
        int CacheGetFd(table_id tid);
        int CacheInvalidate(table_id tid);
        bool ReadHeader(int fd, TableFileHeader *hdr);
        bool WriteHeader(int fd, TableFileHeader *hdr);

    public:
        DiskManager(const char *base_dir);
        ~DiskManager();

        bool CreateTable(table_id tbl_id);
        bool DropTable(table_id tbl_id);
        bool ExistsTable(table_id tbl_id);
        bool ReadPage(table_id tbl_id, page_id pid, void *dest);
        bool WritePage(table_id tbl_id, page_id pid, const void *src);
        bool AllocatePage(table_id tbl_id, page_id *out_page_id);
        bool FreePage(table_id tbl_id, page_id pid);
        bool FlushPage(table_id tbl_id);
        u32 PageCount(table_id tbl_id);
    };
}