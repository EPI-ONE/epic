#ifndef __SRC_UTILS_INCREMENTAL_CONTAINER__
#define __SRC_UTILS_INCREMENTAL_CONTAINER__

#include <unordered_set>

template <typename T>
class Increment {
public:
    Increment() = default;
    Increment(std::unordered_set<T>&& created, std::unordered_set<T>&& removed)
        : created_(std::move(created)), removed_(std::move(removed)) {}
    virtual ~Increment() = default;

    void Create(const T& v) {
        created_.emplace(v);
    }

    void Remove(const T& v) {
        removed_.emplace(v);
    }

    virtual void Merge(Increment&& a) {
        created_.merge(std::move(a.created_));
        for (auto& v : a.removed_) {
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

#endif /* ifndef __SRC_UTILS_INCREMENTAL_CONTAINER__ */
