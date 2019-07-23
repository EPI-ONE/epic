#ifndef __SRC_GETADDR_MESSAGE_H__
#define __SRC_GETADDR_MESSAGE_H__

#include <vector>

#include "block.h"
#include "task.h"

class GetInv {
public:
    // local milestone hashes
    std::vector<uint256> locator;

    // random number to track sync flow
    uint32_t nonce;

    GetInv(std::vector<uint256> locator_, uint32_t nonce_) : locator(std::move(locator_)), nonce(nonce_) {}

    explicit GetInv(VStream& stream) {
        Deserialize(stream);
    }

    void AddBlockHash(uint256 hash) {
        locator.emplace_back(hash);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
        READWRITE(locator);
    }
};

class Inv {
public:
    // max size of inv message
    const static size_t kMaxInventorySize = 1000;

    // milestone hashes
    std::vector<uint256> hashes;

    // random number which corresponds to GetInv message
    uint32_t nonce = 0;

    Inv(std::vector<uint256> hashes_, uint32_t nonce_) : hashes(std::move(hashes_)), nonce(nonce_) {}

    explicit Inv(uint32_t nonce_) : nonce(nonce_) {}

    explicit Inv(VStream& stream) {
        Deserialize(stream);
    }

    bool AddItem(uint256 hash) {
        if (hashes.size() < kMaxInventorySize) {
            hashes.emplace_back(hash);
            return true;
        }
        return false;
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
        READWRITE(hashes);
    }
};

class GetData {
public:
    // data type
    uint8_t type;

    // block hashes
    std::vector<uint256> hashes;

    // random number to track bundle
    std::vector<uint32_t> bundleNonce;

    explicit GetData(GetDataTask::GetDataType type_) : type(type_) {}

    explicit GetData(VStream& stream) {
        Deserialize(stream);
    }

    void AddItem(uint256 hash, uint32_t nonce) {
        hashes.emplace_back(hash);
        bundleNonce.emplace_back(nonce);
    }

    void AddPendingSetNonce(uint32_t nonce) {
        bundleNonce.emplace_back(nonce);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(type);
        READWRITE(hashes);
        READWRITE(bundleNonce);
    }
};

class Bundle {
public:
    Bundle(Bundle&& other) noexcept
        : blocks(std::move(other.blocks)), nonce(other.nonce), payload_(std::move(other.payload_)) {}

    explicit Bundle(VStream& stream) {
        Deserialize(stream);
    }

    explicit Bundle(uint32_t nonce_) : nonce(nonce_) {}

    Bundle(std::vector<ConstBlockPtr> blocks_, uint32_t nonce_) : blocks(std::move(blocks_)), nonce(nonce_) {}

    void AddBlock(const ConstBlockPtr& blockPtr) {
        blocks.push_back(blockPtr);
    }

    void SetPayload(VStream s) {
        payload_ = std::move(s);
    }

    // max block size of a bundle
    constexpr static size_t kMaxBlockSize = 100000;

    // block
    std::vector<ConstBlockPtr> blocks;

    // nonce
    uint32_t nonce;

    template <typename Stream>
    void Serialize(Stream& s) const {
        if (payload_.empty()) {
            s << nonce;
            for (const auto& b : blocks) {
                s << b;
            }
        } else {
            s << nonce << payload_;
        }
    }

    template <typename Stream>
    void Deserialize(Stream& s) {
        s >> nonce;
        while (s.in_avail()) {
            blocks.emplace_back(std::make_shared<const Block>(s));
        }
    }

private:
    VStream payload_;
};

class NotFound {
public:
    explicit NotFound(VStream& stream) {
        Deserialize(stream);
    }

    NotFound(const uint256& hash_, uint32_t nonce_) : hash(hash_), nonce(nonce_) {}

    uint256 hash;
    uint32_t nonce;

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(nonce);
    }
};

#endif // __SRC_GETADDR_MESSAGE_H__
