#include "threadpool.h"
#include <iostream>
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

void ThreadPool::WorkerThread(uint32_t id) {
    try {
        CallableWrapper task;
        while (task_queue_.Take(task)) {
            working_states->at(id) = true;
            task();
            working_states->at(id) = false;
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
        workers_.emplace_back(&ThreadPool::WorkerThread, this, i);
    }
    working_states = new std::vector<std::atomic_bool>(workers_.size());
}

void ThreadPool::Stop() {
    task_queue_.Quit();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t ThreadPool::GetThreadSize() const {
    return workers_.size();
}

size_t ThreadPool::GetTaskSize() const {
    return task_queue_.Size();
}

bool ThreadPool::IsIdle() const {
    if (!task_queue_.Empty()) {
        return false;
    }

    for (int i = 0; i < working_states->size(); i++) {
        if (working_states->at(i)) {
            return false;
        }
    }
    return true;
}

ThreadPool::~ThreadPool() {
    delete working_states;
}
