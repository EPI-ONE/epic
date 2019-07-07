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
    size_ = size;
    workers_.reserve(size_);
}

void ThreadPool::Start() {
    for (size_t i = 0; i < size_; i++) {
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

    if (working_states) {
        delete working_states;
        working_states = nullptr;
    }
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

    if (working_states) {
        for (int i = 0; i < working_states->size(); i++) {
            if (working_states->at(i).load()) {
                return false;
            }
        }
    }

    return task_queue_.Empty();
}

ThreadPool::~ThreadPool() {
    delete working_states;
}
