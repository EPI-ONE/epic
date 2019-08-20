#ifndef __SRC_UTILS_INCREMENTAL_CONTAINER__
#define __SRC_UTILS_INCREMENTAL_CONTAINER__

#include <unordered_set>

#include "big_uint.h"

template <typename T>
class Increment {
public:
    Increment()                 = default;
    Increment(const Increment&) = default;
    Increment(Increment&&)      = default;
    Increment& operator=(const Increment&) = default;
    Increment& operator=(Increment&&) = default;

    Increment(std::unordered_set<T> created, std::unordered_set<T> removed)
        : created_(std::move(created)), removed_(std::move(removed)) {}
    virtual ~Increment() = default;

    void Create(const T& v) {
        if (!removed_.erase(v)) {
            created_.insert(v);
        }
    }

    template <typename... Args>
    void Create(Args&&... args) {
        auto p = T(std::forward<Args>(args)...);
        if (!removed_.erase(p)) {
            created_.emplace(std::move(p));
        }
    }

    void Remove(const T& v) {
        if (!created_.erase(v)) {
            removed_.insert(v);
        }
    }

    template <typename... Args>
    void Remove(Args&&... args) {
        auto p = T(std::forward<Args>(args)...);
        if (!created_.erase(p)) {
            removed_.emplace(std::move(p));
        }
    }

    void Merge(Increment a) {
        created_.merge(std::move(a.created_));
        for (auto&& v : a.removed_) {
            if (!created_.erase(v)) {
                removed_.emplace(v);
            }
        }
    }

    const std::unordered_set<T>& GetCreated() const {
        return created_;
    }

    const std::unordered_set<T>& GetRemoved() const {
        return removed_;
    }

protected:
    std::unordered_set<T> created_;
    std::unordered_set<T> removed_;
};

// Key hasher for std::pair<uint256, uint256>
template <>
struct std::hash<std::pair<uint256, uint256>> {
    size_t operator()(const std::pair<uint256, uint256>& x) const {
        return std::hash<uint256>()(x.first);
    }
};

typedef Increment<std::pair<uint256, uint256>> RegChange;

#endif /* ifndef __SRC_UTILS_INCREMENTAL_CONTAINER__ */
