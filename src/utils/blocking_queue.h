
#ifndef EPIC_BLOCKING_QUEUE_H
#define EPIC_BLOCKING_QUEUE_H


#include <condition_variable>
#include <iostream>
#include <queue>
#include <vector>

#define DEFAULT_CAPACITY 65536

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() : mtx(), full_(), empty_(), capacity_(DEFAULT_CAPACITY) {}

    void Put(T& element) {
        std::unique_lock<std::mutex> lock(mtx);
        full_.wait(lock, [this] { return (queue_.size() < capacity_ || quit_); });
        queue_.emplace(std::move(element));
        empty_.notify_all();
    }

    void Put(T&& element) {
        std::unique_lock<std::mutex> lock(mtx);
        full_.wait(lock, [this] { return (queue_.size() < capacity_ || quit_); });
        queue_.emplace(std::move(element));
        empty_.notify_all();
    }

    bool Take(T& front) {
        std::unique_lock<std::mutex> lock(mtx);
        empty_.wait(lock, [this] { return !queue_.empty() || quit_; });
        if (quit_) {
            return false;
        }
        front = std::move(queue_.front());
        queue_.pop();
        full_.notify_all();
        return true;
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue_.size();
    }

    bool Empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue_.empty();
    }

    void SetCapacity(size_t capacity) {
        capacity_ = capacity;
    }

    void Quit() {
        quit_ = true;
        empty_.notify_all();
        full_.notify_all();
    }

private:
    mutable std::mutex mtx;
    std::condition_variable full_;
    std::condition_variable empty_;
    std::queue<T> queue_;
    size_t capacity_;
    bool quit_ = false;
};


#endif // EPIC_BLOCKING_QUEUE_H
