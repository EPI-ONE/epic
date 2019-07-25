
#ifndef EPIC_SCHEDULER_H
#define EPIC_SCHEDULER_H

#include <chrono>
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
    void Loop() {
        for (auto& t : period_tasks_) {
            t.Run();
        }
    }

    void AddPeriodTask(uint32_t interval, std::function<void()>&& f) {
        period_tasks_.emplace_back(interval, std::move(f));
    }

private:
    std::vector<PeriodTask> period_tasks_;
};


#endif // EPIC_SCHEDULER_H
