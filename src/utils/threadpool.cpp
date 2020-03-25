// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "threadpool.h"
#include "spdlog.h"

CallableWrapper& CallableWrapper::operator=(CallableWrapper&& other) noexcept {
    impl = std::move(other.impl);
    return *this;
}

void CallableWrapper::operator()() {
    impl->Call();
}

ThreadPool::ThreadPool(size_t worker_size) : size_(worker_size), working_states(worker_size) {
    workers_.reserve(size_);
}

void ThreadPool::WorkerThread(uint32_t id) {
#ifdef NDEBUG
    try {
#endif
        bool run;
        do {
            CallableWrapper task;
            run = task_queue_.Take(task);
            if (task_queue_enabled_.load() && run) {
                working_states.at(id) = true;
                task();
                working_states.at(id) = false;
            }
        } while (run);
#ifdef NDEBUG
    } catch (std::exception& e) {
        working_states.at(id) = false;
        workers_.at(id).detach();
        workers_.at(id) = std::thread(std::bind(&ThreadPool::WorkerThread, this, id));
        spdlog::error("\"{}\" thrown in thread pool, restart the thread", e.what());
    }
#endif
}

void ThreadPool::Start() {
    task_queue_.Clear();
    task_queue_.Enable();

    for (size_t i = 0; i < size_; i++) {
        working_states.at(i) = false;
        workers_.emplace_back(&ThreadPool::WorkerThread, this, i);
    }
}

void ThreadPool::Stop() {
    spdlog::debug("Stopping threadPool...");
    task_queue_.Quit();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    spdlog::debug("ThreadPool stopped.");
}

std::size_t ThreadPool::GetThreadSize() const {
    return size_;
}

size_t ThreadPool::GetTaskSize() const {
    return task_queue_.Size();
}

bool ThreadPool::IsIdle() const {
    if (!task_queue_.Empty()) {
        return false;
    }

    for (const auto& s : working_states) {
        if (s.load()) {
            return false;
        }
    }

    return task_queue_.Empty();
}

void ThreadPool::ClearAndDisableTasks() {
    task_queue_enabled_ = false;
    if (!task_queue_.Empty()) {
        task_queue_.Clear();
    }
}

void ThreadPool::Abort() {
    ClearAndDisableTasks();

    while (!IsIdle()) {
        std::this_thread::yield();
    }
    task_queue_enabled_ = true;
}
