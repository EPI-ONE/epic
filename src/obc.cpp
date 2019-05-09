#include "obc.h"

// TODO: add real functions
bool OrphanBlocksContainer::Add(const BlockPtr& b) {
    return false;
}

void OrphanBlocksContainer::Remove(const BlockPtr& b) {}

bool OrphanBlocksContainer::Contains(const uint256& blockHash) const {
    return false;
}

void OrphanBlocksContainer::ReleaseBlocks(const uint256& blockHash) {}
// end of TODO

OrphanBlocksContainer::~OrphanBlocksContainer() {
    thread_.Stop();
    ODict_.clear();
    solidDegree_.clear();
}
