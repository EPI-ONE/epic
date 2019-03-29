
#ifndef EPIC_BLOCKING_QUEUE_H
#define EPIC_BLOCKING_QUEUE_H


#include <iostream>
#include <vector>
#include <queue>
#include <condition_variable>

#define DEFAULT_CAPACITY 65536

template<typename T>
class BlockingQueue{
public:
    BlockingQueue() :mtx(), full_(), empty_(), capacity_(DEFAULT_CAPACITY) { }
    void Put(T& element);
    bool Take(T& front);
    size_t Size();
    bool Empty();
    void SetCapacity(size_t capacity);
    void Quit();

private:
    mutable std::mutex mtx;
    std::condition_variable full_;
    std::condition_variable empty_;
    std::queue<T> queue_;
    size_t capacity_;
    bool quit_ = false;
};


#endif //EPIC_BLOCKING_QUEUE_H
