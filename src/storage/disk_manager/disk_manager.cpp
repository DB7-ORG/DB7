#include "storage/disk_manager/disk_manager.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace db7::storage
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    void DiskManager::BuildPath(table_id tid, char *buf, u32 len)
    {
        int n = snprintf(buf, len, "%s/table_%u.db", base_dir_, (unsigned)tid);
        DB7_ASSERT(n > 0 && (u32)n < len, "Path buffer too small");
    }
#pragma GCC diagnostic pop

    bool DiskManager::ReadHeader(int fd, TableFileHeader *hdr)
    {
        if (pread(fd, hdr, sizeof(*hdr), 0) != sizeof(*hdr))
            return false;
        if (hdr->magic != MAGIC_NUMBER)
        {
            errno = EINVAL;
            return false;
        }
        return true;
    }

    bool DiskManager::WriteHeader(int fd, TableFileHeader *hdr)
    {
        return pwrite(fd, hdr, sizeof(*hdr), 0) == sizeof(*hdr);
    }

    // void cache_invalidate(DiskManager *dm, table_id_t tid)
    // {
    //     FdCacheEntry *entry = cache_find(dm, tid);
    //     if (entry)
    //     {
    //         if (entry->fd >= 0)
    //         {
    //             close(entry->fd);
    //             entry->fd = -1;
    //         }
    //         lru_detach(dm, entry);
    //         dm->cache_size--;
    //     }
    // }

    // void lru_detach(DiskManager *dm, FdCacheEntry *e)
    // {
    //     if (e->lru_prev)
    //         e->lru_prev->lru_next = e->lru_next;
    //     else
    //         dm->lru_head = e->lru_next;

    //     if (e->lru_next)
    //         e->lru_next->lru_prev = e->lru_prev;
    //     else
    //         dm->lru_tail = e->lru_prev;

    //     e->lru_prev = e->lru_next = NULL;
    // }

    // static FdCacheEntry *cache_find(DiskManager *dm, table_id_t tid)
    // {
    //     for (uint32_t i = 0; i < dm->cache_size; i++)
    //     {
    //         if (dm->cache[i].fd >= 0 && dm->cache[i].table_id == tid)
    //         {
    //             return &dm->cache[i];
    //         }
    //     }
    //     return NULL;
    // }

    /*
     * Open (or return cached) OS fd for `table_id`.
     * Handles eviction if we're at capacity.
     * Returns the fd, or -1 on error.
     */
    // static int cache_get_fd(DiskManager *dm, table_id_t tid)
    // {
    //     /* 1. already cached? */
    //     FdCacheEntry *entry = cache_find(dm, tid);
    //     if (entry)
    //     {
    //         lru_touch(dm, entry);
    //         return entry->fd;
    //     }

    //     /* 2. need to open — maybe evict first */
    //     if (dm->cache_size >= MAX_OPEN_FILES)
    //     {
    //         cache_evict_lru(dm);
    //     }

    //     /* 3. find a free slot */
    //     FdCacheEntry *slot = NULL;
    //     for (uint32_t i = 0; i < MAX_OPEN_FILES; i++)
    //     {
    //         if (dm->cache[i].fd < 0)
    //         {
    //             slot = &dm->cache[i];
    //             break;
    //         }
    //     }
    //     if (!slot)
    //     {
    //         /* shouldn't happen after eviction, but be safe */
    //         errno = ENOMEM;
    //         return -1;
    //     }

    //     /* 4. open the file */
    //     char path[MAX_PATH_LEN];
    //     build_path(dm, tid, path, sizeof(path));

    //     int fd = open(path, O_RDWR, 0644);
    //     if (fd < 0)
    //         return -1;

    //     slot->table_id = tid;
    //     slot->fd = fd;
    //     slot->last_access = ++dm->access_counter;
    //     slot->lru_prev = NULL;
    //     slot->lru_next = NULL;

    //     lru_push_front(dm, slot);
    //     dm->cache_size++;

    //     return fd;
    // }

    DiskManager::DiskManager(const char *base_dir)
    {
        memset(this, 0, sizeof(*this));
        strncpy(base_dir_, base_dir, MAX_PATH_LEN - 1);

        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            cache_[i].fd = -1;
        }

        lru_head_ = nullptr;
        lru_tail_ = nullptr;

        mkdir(base_dir_, 0755);
    }

    DiskManager::~DiskManager()
    {
        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            int fd = cache_[i].fd;
            if (fd >= 0)
            {
                fdatasync(fd);
                close(fd);
            }
        }

        // TODO Free LruLinkedList
    }

    bool DiskManager::CreateTable(table_id tbl_id)
    {
        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));

        int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
        if (fd < 0)
        {
            DB7_ASSERT(false, "File already exists");
            return false;
        }

        TableFileHeader hdr;
        hdr.magic = MAGIC_NUMBER;
        hdr.tbl_id = tbl_id;
        hdr.page_count = 1; /* just the header page */
        hdr.free_page_head = 0;

        if (!WriteHeader(fd, &hdr))
        {
            int saved = errno;
            close(fd);
            unlink(path);
            errno = saved;
            return false;
        }

        fsync(fd);
        close(fd);
        return 0;
    }

    bool DiskManager::DropTable(table_id tbl_id)
    {
        // TODO cache_invalidate(table_id)

        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));
        return unlink(path);
    }

    bool DiskManager::ExistsTable(table_id tbl_id)
    {
        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));
        return access(path, F_OK) == 0;
    }

    bool DiskManager::ReadPage(table_id tbl_id, page_id pid, void *dest)
    {
        int fd = 0; // TODO cache_get_fd(dm, table_id);
        if (fd < 0)
            return false;

        off_t offset = (off_t)pid * PAGE_SIZE;
        ssize_t n = pread(fd, dest, PAGE_SIZE, offset);
        if (n != PAGE_SIZE)
        {
            if (n >= 0)
                errno = EIO; /* short read */
            return false;
        }
        return true;
    }

    bool DiskManager::WritePage(table_id tbl_id, page_id pid, const void *src)
    {
        int fd = 0; // TODO cache_get_fd(dm, table_id);
        if (fd < 0)
            return false;

        off_t offset = (off_t)pid * PAGE_SIZE;
        ssize_t n = pwrite(fd, src, PAGE_SIZE, offset);
        if (n != PAGE_SIZE)
        {
            if (n >= 0)
                errno = EIO; /* short write */
            return false;
        }
        return true;
    }

    bool DiskManager::AllocatePage(table_id tbl_id, page_id *out_page_id)
    {
        int fd = 0; // TODO cache_get_fd(dm, table_id);
        if (fd < 0)
            return false;

        TableFileHeader hdr; // TODO this is wierd not going over buffer pool
        if (ReadHeader(fd, &hdr) < 0)
            return false;

        if (hdr.free_page_head != 0)
        {
            /*
             * Reclaim from free list.
             * The first 4 bytes of a free page store the next free page_id
             * (0 = end of list).
             */
            page_id reclaimed = hdr.free_page_head;

            uint32_t next_free = 0;
            off_t off = (off_t)reclaimed * PAGE_SIZE; // TODO this mechanism doesnt seem thread safe
            if (pread(fd, &next_free, sizeof(next_free), off) != sizeof(next_free))
                return false;

            hdr.free_page_head = next_free;
            if (WriteHeader(fd, &hdr) < 0)
                return false;

            char zeros[PAGE_SIZE]; // TODO this can be cached
            memset(zeros, 0, PAGE_SIZE);
            if (pwrite(fd, zeros, PAGE_SIZE, off) != PAGE_SIZE)
                return false;

            *out_page_id = reclaimed;
        }
        else
        {
            page_id new_page = hdr.page_count;

            char zeros[PAGE_SIZE]; // TODO this can be cached
            memset(zeros, 0, PAGE_SIZE);
            off_t off = (off_t)new_page * PAGE_SIZE;
            if (pwrite(fd, zeros, PAGE_SIZE, off) != PAGE_SIZE)
                return false;

            hdr.page_count = new_page + 1;
            if (WriteHeader(fd, &hdr) < 0)
                return false;

            *out_page_id = new_page;
        }

        return true;
    }

    bool DiskManager::FreePage(table_id tbl_id, page_id page_id)
    {
        int fd = 0; // TODO cache_get_fd(dm, table_id);
        if (fd < 0)
            return false;

        TableFileHeader hdr;
        if (ReadHeader(fd, &hdr) < 0)
            return false;

        if (page_id == 0 || page_id >= hdr.page_count)
        {
            errno = EINVAL;
            return false;
        }

        /*
         * Push onto free list: write the current head into the first 4 bytes
         * of the freed page, then update the header.
         */
        uint32_t old_head = hdr.free_page_head;
        off_t off = (off_t)page_id * PAGE_SIZE;

        /* zero the page first, then write the free-list pointer */
        char zeros[PAGE_SIZE];
        memset(zeros, 0, PAGE_SIZE); // TODO cache this
        memcpy(zeros, &old_head, sizeof(old_head));
        if (pwrite(fd, zeros, PAGE_SIZE, off) != PAGE_SIZE)
            return false;

        hdr.free_page_head = page_id;
        if (WriteHeader(fd, &hdr) < 0)
            return false;

        return true;
    }

    bool DiskManager::FlushPage(table_id tbl_id)
    {
        FdCacheEntry *entry = nullptr; // TODO cache_find(dm, table_id);
        if (entry && entry->fd >= 0)
            return fsync(entry->fd);
        return true; /* not cached = nothing to flush */
    }

    u32 DiskManager::PageCount(table_id tbl_id)
    {
        int fd = 0; // TODO cache_get_fd(dm, table_id);
        if (fd < 0)
            return 0;

        TableFileHeader hdr;
        if (ReadHeader(fd, &hdr) < 0)
            return 0;

        return hdr.page_count;
    }
}