#ifndef __SRC_OBC_H__
#define __SRC_OBC_H__

#include "block.h"

#include <map>
#include <shared_mutex>
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
    /*
     * constructs:
     * 1: initializes block_dep_map
     * 2: initializes lose_ends map
     */
    OrphanBlocksContainer() = default;

    /*
     * clears all maps
     */
    ~OrphanBlocksContainer();

    /*
     * returns the number of missing
     * hashes necessary to empty the OBC
     */
    size_t Size() const;

    /*
     * returns the number dependencies
     * managed by the OBC
     */
    size_t DependencySize() const;

    bool IsEmpty() const;

    bool Contains(const uint256& hash) const;

    /*
     * adds block to OBC
     */
    void AddBlock(const ConstBlockPtr& block, uint8_t missing_mask);

    /*
     * Submits the information that a new block with given hash is available
     * to the OBC solver which then ties up as many lose ends as possible
     * with this information
     */
    std::optional<std::vector<ConstBlockPtr>> SubmitHash(const uint256& hash);

private:
    struct obc_dependency {
        /* number of dependencies that must be
         * found in order for this dependency
         * to be resolved; max: 3 & min: 0 */
        uint_fast8_t ndeps;
        /* links to other dependencies that wait
         * for this one to be resolved*/
        std::vector<std::shared_ptr<struct obc_dependency>> deps;
        /* pointer to the block that is the
         * actual orphan */
        ConstBlockPtr block;
    };

    using obc_dep     = struct obc_dependency;
    using obc_dep_ptr = std::shared_ptr<obc_dep>;

    mutable std::shared_mutex mutex_;
    /* this container maps the hash of the orphan block
     * to its dependency struct */
    std::unordered_map<uint256, obc_dep_ptr> block_dep_map_;
    /* this container maps missing hashes to one or more
     * dependency structs that wait for this one hash */
    std::unordered_map<uint256, std::unordered_set<obc_dep_ptr>> lose_ends_;
};

#endif // __SRC_OBC_H__
