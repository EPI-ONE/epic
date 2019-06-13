#ifndef __SRC_TASK_H_
#define __SRC_TASK_H_

#include <atomic>
#include <memory>

static uint32_t GetNewNonce() {
    static std::atomic_uint_fast32_t nonce_ = 0;
    return nonce_.fetch_add(1, std::memory_order_relaxed);
}

class Peer;
class Task {
public:
    explicit Task() : id(GetNewNonce()), timeout(0), peer_() {}

    // explict requires copy constructor of peer to increase the ref of shared_prt<Peer>
    void SetPeer(std::shared_ptr<Peer> peer) {
        peer_ = peer;
    }

    std::shared_ptr<Peer> GetPeer() const {
        return peer_.lock();
    }

    uint32_t id;

    uint64_t timeout;

private:
    std::weak_ptr<Peer> peer_;
};

class GetInvTask : public Task {
public:
    explicit GetInvTask() : Task() {}
};

class GetDataTask : public Task {
public:
    enum GetDataType { LEVEL_SET = 1, VALID_SET, PENDING_SET };

    GetDataTask(GetDataType type_) : Task(), type(type_) {}

    GetDataType type;
};

#endif // ifndef __SRC_TASK_H_
