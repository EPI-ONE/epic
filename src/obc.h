#ifndef __SRC_OBC_H__
#define __SRC_OBC_H__

#include <map>
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
     * this->size() == 0
     */
    bool Empty() const;

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
    void AddBlock(const BlockPtr& block, bool m_missing, bool t_missing, bool p_missing);

    /*
     * Submits a task to thread_ that releases any block from OBC
     * that becomes solid with blockHash being its antecedent.
     */
    std::optional<std::vector<BlockPtr>> SubmitHash(const uint256& hash);

private:
    struct obc_dependency {
        uint_fast8_t ndeps;
        std::vector<std::shared_ptr<struct obc_dependency>> deps;
        BlockPtr block;
    };

    typedef struct obc_dependency obc_dep;
    typedef std::shared_ptr<obc_dep> obc_dep_ptr;

    std::unordered_map<uint256, obc_dep_ptr> block_dep_map_;
    std::multimap<uint256, obc_dep_ptr> lose_ends_;
};


#endif
