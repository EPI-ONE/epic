#include "threadpool.h"

CallableWrapper& CallableWrapper::operator=(CallableWrapper&& other) noexcept {
    impl = std::move(other.impl);
    return *this;
}

void CallableWrapper::operator()() {
    impl->Call();
}

ThreadPool::ThreadPool(size_t worker_size) {
    SetThreadSize(worker_size);
}

void ThreadPool::WorkerThread() {
    try {
        CallableWrapper task;
        while (task_queue_.Take(task)) {
            task();
        }
    } catch (std::exception& e) {
        spdlog::error(e.what());
    }
}

void ThreadPool::SetThreadSize(size_t size) {
    workers_.reserve(size);
}

void ThreadPool::Start() {
    for (size_t i = 0; i < workers_.capacity(); i++) {
        workers_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

void ThreadPool::Stop() {
    task_queue_.Quit();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
}

std::size_t ThreadPool::size() const {
    return workers_.size();
}

size_t ThreadPool::GetTaskSize() const {
    return task_queue_.Size();
}
