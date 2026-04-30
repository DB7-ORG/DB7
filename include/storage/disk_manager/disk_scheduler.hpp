#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"
#include "storage/page.hpp"
#undef BLOCK_SIZE
#include "third_party/concurrentqueue.h"

#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace db7::storage
{
    enum class IoPriority : u8
    {
        CRITICAL, // WAL flush, transaction commit
        HIGH,     // page reads for active queries
        LOW       // background tasks, prefetch
    };

    struct IoTask
    {
        enum Op
        {
            READ,
            WRITE
        };
        Op op;
        IoPriority priority;
        Page *page;
        PageIdentifier id;

        IoTask() {}

        IoTask(Op op, IoPriority priority, Page *page, PageIdentifier id)
            : op(op), priority(priority), page(page), id(id) {}

        // higher priority = should come first
        bool operator<(const IoTask &other) const
        {
            return priority > other.priority; // reversed for min-heap
        }
    };

    class DiskScheduler
    {
    private:
        DiskManagerAsync *io_;
        u32 inflight_;
        u32 max_inflight_;
        std::atomic<bool> running_;

        moodycamel::ConcurrentQueue<IoTask> queue_;
        std::thread worker_;

        void Run()
        {
            io_->SetCompletionCallback(
                [](void *user_data, ssize_t result)
                {
                    (void)result;
                    auto *page = static_cast<Page *>(user_data);
                    DB7_ASSERT(result == PAGE_SIZE, "Short read");
                    DB7_ASSERT(page->IsIOInProgress(), "Io in progress not set");

                    DB7_ASSERT(page->GetId().pid == *(u64 *)page->GetData(), "Invalid page");
                    // printf("%d ", page->GetId().pid);

                    // TODO DB7_ASSERT(page->IsPinned(), "Pin not set");
                    // Dont need locks here since no page can write to header while io_in_progress is set

                    page->SignalIO();
                });

            while (running_)
            {
                // 1. drain completions if any are ready
                DrainCompletions();

                // 2. submit as many queued tasks as inflight budget allows
                SubmitPending();

                // 3. if nothing to do, sleep
                if (queue_.size_approx() == 0 && inflight_ == 0)
                {
                    // Nothing to do — back off briefly
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }

            // drain remaining on shutdown
            while (inflight_ > 0)
                DrainCompletions();
        }

        void SubmitPending()
        {
            bool submitted_any = false;

            // Bulk dequeue up to available inflight slots
            u32 slots = std::min((u32)IOURING_QUEUE_SIZE, max_inflight_ - inflight_);
            if (slots == 0)
                return;
            IoTask tasks[IOURING_QUEUE_SIZE]; // or use max_inflight_ with a vector if it varies
            size_t count = queue_.try_dequeue_bulk(tasks, slots);

            for (size_t i = 0; i < count; ++i)
            {
                bool ok;
                if (tasks[i].op == IoTask::READ)
                    ok = io_->SubmitRead(tasks[i].id.tbl_id, tasks[i].page->GetData(), PAGE_SIZE, tasks[i].id.pid * PAGE_SIZE, (void *)tasks[i].page);
                else
                    ok = io_->SubmitWrite(tasks[i].id.tbl_id, tasks[i].page->GetData(), PAGE_SIZE, tasks[i].id.pid * PAGE_SIZE, (void *)tasks[i].page);

                if (ok)
                {
                    inflight_++;
                    submitted_any = true;
                }
                else
                {
                    // Ring full — re-enqueue this task and all remaining ones
                    queue_.enqueue_bulk(tasks + i, count - i);
                    break;
                }
            }

            if (submitted_any)
                io_->Submit();
        }

        void DrainCompletions()
        {
            if (inflight_ > 0)
            {
                inflight_ -= io_->ReapCompletions(max_inflight_);
            }
        }

    public:
        DiskScheduler(DiskManagerAsync *io)
            : io_(io), inflight_(0), max_inflight_(IOURING_QUEUE_SIZE), running_(false), queue_(IOURING_QUEUE_SIZE) {}

        ~DiskScheduler() { Stop(); }

        void Start()
        {
            running_ = true;
            worker_ = std::thread(&DiskScheduler::Run, this);
        }

        void Stop()
        {
            running_ = false;
            if (worker_.joinable())
                worker_.join();
        }

        void Enqueue(IoTask task)
        {
            queue_.enqueue(task);
        }
    };
}