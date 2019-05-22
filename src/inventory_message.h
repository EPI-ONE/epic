#ifndef EPIC_INVENTORY_MESSAGE_H
#define EPIC_INVENTORY_MESSAGE_H

#include <vector>

#include "serialize.h"

class Inv {
public:
    explicit Inv(std::vector<uint256>& hashes_, uint32_t nonce_) : milestoneHashes(std::move(hashes_)), nonce(nonce_) {}

    explicit Inv(uint32_t nonce_) : nonce(nonce_) {}

    explicit Inv(VStream& stream){
        Deserialize(stream);
    }

    void AddBlockHash(uint256 hash) {
        if (milestoneHashes.size() < kMaxInventorySize) {
            milestoneHashes.emplace_back(hash);
        }
    }

    // max size of inv message
    const static size_t kMaxInventorySize = 1000;

    // milestone hashes
    std::vector<uint256> milestoneHashes;

    // random number which corresponds to GetBlock message
    uint32_t nonce = 0;


    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(milestoneHashes);
        READWRITE(nonce);
    }
};
#endif // EPIC_INVENTORY_MESSAGE_H
