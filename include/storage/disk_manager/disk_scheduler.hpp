#pragma once

#include "storage/disk_manager/disk_manager_async.hpp"

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
        int fd;
        void *buf;
        u32 len;
        off_t offset;
        void *user_data;

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
        u32 max_inflight_;
        std::atomic<u32> inflight_;
        std::atomic<bool> running_;

        std::priority_queue<IoTask> queue_;
        std::mutex mu_;
        std::condition_variable cv_;
        std::thread worker_;

        void Run()
        {
            io_->SetCompletionCallback([this](void *ud, ssize_t res)
                                       {
            inflight_--;
            // forward to whoever submitted the task
            auto *task = static_cast<IoTask *>(ud);
            // handle result here — notify waiting thread, mark page ready, etc.
            (void)res;
            delete task; });

            while (running_)
            {
                // 1. drain completions if any are ready
                DrainCompletions();

                // 2. submit as many queued tasks as inflight budget allows
                SubmitPending();

                // 3. if nothing to do, sleep
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait_for(lock, std::chrono::milliseconds(1), [this]
                             { return !queue_.empty() || !running_; });
            }

            // drain remaining on shutdown
            while (inflight_ > 0)
                DrainCompletions();
        }

        void SubmitPending()
        {
            std::lock_guard<std::mutex> lock(mu_);
            bool submitted_any = false;

            while (!queue_.empty() && inflight_ < max_inflight_)
            {
                IoTask task = queue_.top();
                queue_.pop();

                // heap-allocate so pointer survives until completion
                auto *t = new IoTask(task);

                bool ok;
                if (task.op == IoTask::READ)
                    ok = io_->SubmitRead(task.fd, task.buf, task.len, task.offset, t);
                else
                    ok = io_->SubmitWrite(task.fd, task.buf, task.len, task.offset, t);

                if (ok)
                {
                    inflight_++;
                    submitted_any = true;
                }
                else
                {
                    // ring full, put it back
                    queue_.push(task);
                    delete t;
                    break;
                }
            }

            if (submitted_any)
                io_->Submit();
        }

        void DrainCompletions()
        {
            if (inflight_ > 0)
                io_->ReapCompletions(max_inflight_);
        }

    public:
        DiskScheduler(DiskManagerAsync *io, u32 max_inflight = 64)
            : io_(io), max_inflight_(max_inflight), inflight_(0), running_(false)
        {
        }

        ~DiskScheduler() { Stop(); }

        void Start()
        {
            running_ = true;
            worker_ = std::thread(&DiskScheduler::Run, this);
        }

        void Stop()
        {
            running_ = false;
            cv_.notify_one();
            if (worker_.joinable())
                worker_.join();
        }

        void Enqueue(IoTask task)
        {
            {
                std::lock_guard<std::mutex> lock(mu_);
                queue_.push(std::move(task));
            }
            cv_.notify_one();
        }
    };
}