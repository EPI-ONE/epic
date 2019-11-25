#ifndef EPIC_OBC_H
#define EPIC_OBC_H

#include "block.h"

#include <map>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* bitmask for indicating missing dependencies */
enum OBC_DEP_STATUS : uint8_t {
    M_MISSING = 1 << 0,
    T_MISSING = 1 << 1,
    P_MISSING = 1 << 2,
};

class OrphanBlocksContainer {
public:
    /**
     * constructs:
     * 1: initializes block_dep_map
     * 2: initializes lose_ends map
     */
    OrphanBlocksContainer() = default;

    /**
     * returns the number of missing
     * hashes necessary to empty the OBC
     */
    size_t Size() const;

    bool Empty() const;

    bool Contains(const uint256& hash) const;

    /**
     * adds block to OBC
     */
    void AddBlock(ConstBlockPtr&& block, uint8_t missing_mask);

    /**
     * Submits the information that a new block with given hash is available
     * to the OBC solver which then ties up as many lose ends as possible
     * with this information
     */
    std::vector<ConstBlockPtr> SubmitHash(const uint256& hash);

    void DeleteBlockTree(const uint256& hash);

    /**
     * Iterate block_dep_map_ and delete outdated entries that have timestamps
     * before the current time - specified seconds.
     */
    size_t Prune(uint32_t seconds);

    size_t GetDepNodeSize() const;

private:
    size_t obcBlockSize_ = 0;
    struct obc_dependency {
        // number of dependencies that must be
        // found in order for this dependency
        // to be resolved; max: 3 & min: 0
        uint_fast8_t ndeps = 0;
        // links to other dependencies that wait
        // for this one to be resolved
        std::unordered_set<std::shared_ptr<struct obc_dependency>> deps;
        // pointer to the block that is the actual orphan
        ConstBlockPtr block = nullptr;
    };

    using obc_dep     = struct obc_dependency;
    using obc_dep_ptr = std::shared_ptr<obc_dep>;

    mutable std::shared_mutex mutex_;

    /**
     * this container maps the hash of the orphan block
     * to its dependency struct
     */
    std::unordered_map<uint256, obc_dep_ptr> block_dep_map_;
};

#endif // EPIC_OBC_H
