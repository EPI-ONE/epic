#ifndef __SRC_OBC_H__
#define __SRC_OBC_H__

#include <map>
#include <unordered_set>
#include <vector>

#include "block.h"

/*!! NOT THREAD SAFE !!*/
class OrphanBlocksContainer {
public:
    /*
     * constructs:
     * 1: initializes block_dep_map
     * 2: initializes lose_ends map
     */
    OrphanBlocksContainer() = default;

    /*
     * clear all maps
     */
    ~OrphanBlocksContainer();

    /*
     * returns the number of missing
     * hashes necessary to empty the OBC
     */
    size_t Size() const;

    /*
     * returns the number dependencies
     * managed by the OBC */
    size_t DependencySize() const;

    /*
     * if there are no blocks managed
     */
    bool IsEmpty() const;

    /*
     * checks if a block is an orphan
     */
    bool Contains(const uint256& hash) const;

    /*
     * adds block to OBC
     * m_missing: true -> milestone is missing
     * t_missing: true -> tip is missing
     * p_missing: true -> prev is missing
     */
    void AddBlock(const ConstBlockPtr& block, bool m_missing, bool t_missing, bool p_missing);

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

    typedef struct obc_dependency obc_dep;
    typedef std::shared_ptr<obc_dep> obc_dep_ptr;

    /* this container maps the hash of the orphan block
     * to its dependency struct */
    std::unordered_map<uint256, obc_dep_ptr> block_dep_map_;
    /* this container maps missing hashes to one or more
     * dependency structs that wait for this one hash */
    std::multimap<uint256, obc_dep_ptr> lose_ends_;

    /* calculates the number of unique missing dependencies
     * based on the hashes of the missing blocks */
    static uint8_t DependencyCardinality(ConstBlockPtr block, bool m_missing, bool t_missing, bool p_missing);
};

#endif
