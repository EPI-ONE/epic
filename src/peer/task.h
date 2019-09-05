// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_TASK_H
#define EPIC_TASK_H

#include "sync_messages.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

static uint32_t GetNewNonce() {
    static std::atomic_uint_fast32_t nonce_ = 0;
    return nonce_.fetch_add(1, std::memory_order_relaxed);
}

class Task {
public:
    Task(uint32_t timeout) : nonce(GetNewNonce()) {
        timeout_ = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
    }

    bool IsTimeout() {
        if (completed_) {
            return false;
        }
        return std::chrono::steady_clock::now() > timeout_;
    }

    void Complete() {
        completed_ = true;
    }

    uint32_t nonce;

private:
    std::chrono::time_point<std::chrono::steady_clock> timeout_; // in seconds
    std::atomic_bool completed_ = false;
};

class GetInvTask : public Task {
public:
    GetInvTask(uint32_t timeout) : Task(timeout) {}
};

class GetDataTask : public Task {
public:
    enum GetDataType { LEVEL_SET = 1, PENDING_SET };

    GetDataTask(GetDataType t, uint256& h, uint32_t timeout) : Task(timeout), type(t), hash(h) {}
    GetDataTask(GetDataType t, uint32_t timeout) : Task(timeout), type(t) {}

    GetDataType type;
    std::shared_ptr<GetDataTask> next = nullptr;
    std::shared_ptr<Bundle> bundle    = nullptr;
    uint256 hash;
};

class GetDataTaskManager {
public:
    void Push(std::shared_ptr<GetDataTask> task) {
        std::unique_lock<std::shared_mutex> writer(mutex_);
        if (tasks_.empty()) {
            head_ = task;
            tail_ = task;
        } else {
            tail_->next = task;
            tail_       = task;
        }
        tasks_.insert_or_assign(task->nonce, task);
    }

    std::shared_ptr<GetDataTask>& Front() {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        return head_;
    }

    void Pop() {
        std::unique_lock<std::shared_mutex> writer(mutex_);
        tasks_.erase(head_->nonce);

        if (head_ == tail_) {
            head_ = nullptr;
            tail_ = nullptr;
        } else {
            head_ = head_->next;
        }
    }

    bool Empty() {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        return tasks_.empty();
    }

    size_t Size() {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        return tasks_.size();
    }

    bool IsTimeout() {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        for (auto& t : tasks_) {
            if (t.second->IsTimeout()) {
                return true;
            }
        }

        return false;
    }

    bool CompleteTask(std::shared_ptr<Bundle> bundle) {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        auto task = tasks_.find(bundle->nonce);
        if (task != tasks_.end()) {
            task->second->bundle = bundle;
            task->second->Complete();
            return true;
        }
        return false;
    }

    std::vector<std::shared_ptr<GetDataTask>> GetTasks() {
        std::shared_lock<std::shared_mutex> reader(mutex_);
        auto result = std::vector<std::shared_ptr<GetDataTask>>();
        for (auto& task : tasks_) {
            if (task.second->type == GetDataTask::LEVEL_SET) {
                result.push_back(task.second);
            }
        }
        return result;
    }

private:
    std::shared_mutex mutex_;
    std::unordered_map<uint32_t, std::shared_ptr<GetDataTask>> tasks_;
    std::shared_ptr<GetDataTask> head_ = nullptr;
    std::shared_ptr<GetDataTask> tail_ = nullptr;
};

#endif // EPIC_TASK_H
