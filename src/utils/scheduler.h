// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SCHEDULER_H
#define EPIC_SCHEDULER_H

#include "spdlog/spdlog.h"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

class PeriodTask {
public:
    PeriodTask(uint32_t interval, std::function<void()>&& f) : interval_(interval), f_(std::move(f)) {
        next_ = std::chrono::steady_clock::now() + std::chrono::seconds(interval_);
    }

    void Run() {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_) {
            next_ = std::chrono::steady_clock::now() + std::chrono::seconds(interval_);
            f_();
        }
    }

private:
    uint32_t interval_;
    std::chrono::time_point<std::chrono::steady_clock> next_;
    std::function<void()> f_;
};

class Scheduler {
public:
    void Start() {
        thread = std::thread(std::bind(&Scheduler::Loop, this));
    }

    void Stop() {
        interrupt_ = true;
        if (thread.joinable()) {
            thread.join();
        }
    }

    void AddPeriodTask(uint32_t interval, std::function<void()>&& f) {
        period_tasks_.emplace_back(interval, std::move(f));
    }

private:
    void Loop() {
        while (!interrupt_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (auto& t : period_tasks_) {
                t.Run();
            }
        }
    }
    std::vector<PeriodTask> period_tasks_;
    std::thread thread;
    std::atomic_bool interrupt_ = false;
};

class Timer {
public:
    Timer(uint32_t _duration, std::function<void()>&& f) : duration_(_duration), f_(std::move(f)), stop_(true) {
        if (duration_ == 0) {
            spdlog::trace("Created a time of invalid duration");
        }
    }
    ~Timer() {
        Stop();
    }

    void Reset() {
        if (duration_ == 0) {
            return;
        }

        Stop();
        stop_   = false;
        thread_ = std::thread{[&, end = std::chrono::steady_clock::now() + std::chrono::seconds{duration_}]() {
            while (!stop_ && std::chrono::steady_clock::now() < end) {
                std::this_thread::sleep_for(std::chrono::milliseconds{500});
            }
            f_();
        }};
    }

    void Stop() {
        if (thread_.joinable()) {
            stop_ = true;
            thread_.join();
        }
    }

private:
    uint32_t duration_;
    std::function<void()> f_;
    std::atomic_bool stop_;
    std::thread thread_;
};

#endif // EPIC_SCHEDULER_H
