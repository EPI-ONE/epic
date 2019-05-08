#ifndef EPIC_THREADPOOL_H
#define EPIC_THREADPOOL_H

#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "blocking_queue.h"
#include "spdlog.h"

class CallableWrapper {
public:
    template <typename F>
    CallableWrapper(F&& f) : impl(new CallableImpl<F>(std::forward<F>(f))) {}

    CallableWrapper() = default;

    explicit CallableWrapper(CallableWrapper&& other) noexcept : impl(std::move(other.impl)) {}

    CallableWrapper& operator=(CallableWrapper&& other) noexcept;

    void operator()();

    CallableWrapper(CallableWrapper& other)       = delete;
    CallableWrapper(const CallableWrapper& other) = delete;
    CallableWrapper& operator=(CallableWrapper& other) = delete;

private:
    struct Callable {
        virtual void Call() = 0;
        virtual ~Callable() = default;
    };

    template <typename F>
    struct CallableImpl : Callable {
        F f;
        explicit CallableImpl(F&& other) : f(std::move((other))) {}
        void Call() override {
            f();
        }
    };

    std::unique_ptr<struct Callable> impl;
};


class ThreadPool {
public:
    explicit ThreadPool(size_t worker_size);

    ThreadPool() = default;

    void SetThreadSize(size_t size);

    void Start();

    void Stop();

    std::size_t size() const;

    /**
     * execute a callable which has the operator () without return value, if you want to pass the arguments,
     * you should use std::bind or lambda function
     * @tparam FunctionType
     * @param f
     */
    template <typename FunctionType>
    void Execute(FunctionType&& f) {
        task_queue_.Put(std::move(f));
    }

    /**
     * execute a callable which has the operator () and return the std::future, if you want to pass the arguments,
     * you should use std::bind or lambda function
     * @tparam FunctionType
     * @param f
     * @return
     */
    template <typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> Submit(FunctionType&& f) {
        typedef typename std::result_of<FunctionType()>::type result_type;
        std::packaged_task<result_type()> packagedTask(std::move(f));
        std::future<result_type> result(packagedTask.get_future());
        task_queue_.Put(std::move(packagedTask));
        return result;
    }

private:
    BlockingQueue<CallableWrapper> task_queue_;
    std::vector<std::thread> workers_;

    void WorkerThread();
};

#endif // EPIC_THREADPOOL_H
