#include "blocking_queue.h"
#include "net_address.h"
#include "net_message.h"

template <typename T>
void BlockingQueue<T>::Put(T& element) {
    std::unique_lock<std::mutex> lock(mtx);
    full_.wait(lock, [this] { return (queue_.size() < capacity_ || quit_); });
    queue_.emplace(std::move(element));
    empty_.notify_all();
}

template <typename T>
bool BlockingQueue<T>::Take(T& front) {
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

template <typename T>
size_t BlockingQueue<T>::Size() {
    std::lock_guard<std::mutex> lock(mtx);
    return queue_.size();
}

template <typename T>
bool BlockingQueue<T>::Empty() {
    std::lock_guard<std::mutex> lock(mtx);
    return queue_.empty();
}

template <typename T>
void BlockingQueue<T>::SetCapacity(size_t capacity) {
    capacity_ = capacity;
}

template <typename T>
void BlockingQueue<T>::Quit() {
    quit_ = true;
    empty_.notify_all();
    full_.notify_all();
}

template class BlockingQueue<NetMessage>;
template class BlockingQueue<NetAddress>;