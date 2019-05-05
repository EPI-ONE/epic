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
        while (task_queue.Take(task)) {
            task();
        }
    } catch (std::exception& e) {
        spdlog::error(e.what());
    }
}

void ThreadPool::SetThreadSize(size_t size) {
    workers.reserve(size);
}

void ThreadPool::Start() {
    for (int i = 0; i < workers.capacity(); i++) {
        workers.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

void ThreadPool::Stop() {
    task_queue.Quit();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers.clear();
}
