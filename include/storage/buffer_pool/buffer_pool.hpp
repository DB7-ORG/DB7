#pragma once

#include "common.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager/disk_manager.hpp"

namespace db7::storage
{
    /**
     * I have 128 partitions(one hash table for each) instead of single hash table which will limit concurrency
     *
     * Pinning a page:
     * - Once i want to get a page into memory ill first need to decide which partitions it belongs to.
     * - Hash the key(upper bits determine the partition)
     * - Take a latch on the partition map and search the map for that key.
     * -(fast path)- If key exists (fast path) GREAT aquire page latch and then increment refcount then release partition latch .
     * -(slow path)- If not  release a latch and using eviction strategy find a victim to replace (there are tricks where u can flush dirty pages).
     *               Once u find a victim aquire a latch to partition again insert a new one and delete old key. Note that order is important u first try to insert a key so
     *               if it exists someone else got the page in buffer pool and u can just reuse their work and throw away yours.
     * - Then work with ur page by aquiring a latch and increasing a refcount.
     */

    /** TODO
     * The description is mostly correct but there are a few things to tighten up.
Pin before latch. When you find the page on the fast path, you should increment the ref count first, then release the partition latch, then take the page content latch. If you take the page latch while still holding the partition latch, you're holding two locks simultaneously which increases contention and creates deadlock potential. The ref count alone is enough to guarantee the page won't be evicted — you don't need the partition latch for that protection.
Victim pinning is missing. In the slow path you mention finding a victim, but you don't mention pinning it. Between finding a victim and re-acquiring the partition latch, someone else might start using that page. You need to pin the victim (increment its ref count) before releasing the eviction structure lock, so nobody else can evict it out from under you.
Dirty page flush is glossed over. The comment says "there are tricks where u can flush dirty pages" but this is actually a critical part of the flow. If the victim is dirty, you flush it to disk while holding only the pin, no partition latch. This deserves explicit mention since getting it wrong causes major problems.
The delete-then-insert ordering is slightly off. You said insert first then delete, which is right — you try to insert your new key to check for conflicts. But the old victim's entry might be in a completely different partition. So you might need to lock two partitions: the victim's partition to delete its entry and your target partition to insert the new entry. That lock ordering needs to be consistent (always lock lower partition number first, for example) or you'll deadlock.
Here's a revised version:
     */
    class BufferPool
    {
    private:
        DiskManager *disk_mng_;
        Page *pages_;
        u32 poolSize_;

    public:
        BufferPool(DiskManager *disk_mng);
        Page *Pin(u32 pid); // TODO all of these should have private methods calling DiskManager
        void Unpin(u32 pid, bool dirty);
        void Flush(u32 pid);
    };
}