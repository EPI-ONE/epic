#ifndef __SRC_TASK_H_
#define __SRC_TASK_H_
#include <memory>

class Peer;

class Task {
public:
    explicit Task(uint32_t nonce) : id(nonce), timeout(0), peer_(nullptr) {}

    // explict requires copy constructor of peer to increase the ref of shared_prt<Peer>
    void SetPeer(std::shared_ptr<Peer> peer) {
        peer_ = peer;
    }

    std::shared_ptr<Peer>& GetPeer() {
        return peer_;
    }

    uint32_t id;

    uint64_t timeout;

private:
    std::shared_ptr<Peer> peer_;
};

class GetDataTask : public Task {
public:
    enum GetDataType { LEVEL_SET = 1, VALID_SET, PENDING_SET };

    GetDataTask(uint32_t nonce, GetDataType type_) : Task(nonce), type(type_) {}

    GetDataType type;
};

class GetBlockTask : public Task {
public:
    explicit GetBlockTask(uint32_t nonce) : Task(nonce) {}
};

#endif // ifndef __SRC_TASK_H_
