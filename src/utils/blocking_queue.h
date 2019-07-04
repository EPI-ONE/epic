#ifndef EPIC_BLOCKING_QUEUE_H
#define EPIC_BLOCKING_QUEUE_H

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <queue>
#include <vector>

#define DEFAULT_CAPACITY 1 << 16

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() : mtx_(), full_(), empty_(), capacity_(DEFAULT_CAPACITY) {}

    void Put(T& element) {
        std::unique_lock<std::mutex> lock(mtx_);
        full_.wait(lock, [this] { return (queue_.size() < capacity_ || quit_); });
        queue_.emplace(std::move(element));
        empty_.notify_all();
    }

    void Put(T&& element) {
        std::unique_lock<std::mutex> lock(mtx_);
        full_.wait(lock, [this] { return (queue_.size() < capacity_ || quit_); });
        queue_.emplace(std::move(element));
        empty_.notify_all();
    }

    bool Take(T& front) {
        std::unique_lock<std::mutex> lock(mtx_);
        empty_.wait(lock, [this] { return !queue_.empty() || quit_; });
        if (quit_) {
            return false;
        }
        front = std::move(queue_.front());
        queue_.pop();
        full_.notify_all();
        return true;
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    void SetCapacity(size_t capacity) {
        capacity_ = capacity;
    }

    void Quit() {
        std::lock_guard<std::mutex> lock(mtx_);
        quit_ = true;
        full_.notify_all();
        empty_.notify_all();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<T> empty;
        std::swap(queue_, empty);
        full_.notify_all();
    }

private:
    mutable std::mutex mtx_;
    mutable std::mutex drainMtx_;
    std::condition_variable full_;
    std::condition_variable empty_;
    std::queue<T> queue_;
    size_t capacity_;
    std::atomic_bool quit_ = false;
};

/**
 * Moves the first n elements from a BlockingQueue to a vector.
 * Not thread safe.
 */
template<typename T>
bool DrainTo(BlockingQueue<T>& src, std::vector<T>& dest, size_t n) {
    for (auto i = 0; i < n; ++i) {
        dest.emplace_back();
        if (!src.Take(dest.back())) {
            dest.pop_back();
            return false;
        }
    }
    return true;
}

#endif // EPIC_BLOCKING_QUEUE_H
