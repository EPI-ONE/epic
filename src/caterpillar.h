#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include "block.h"
#include "uint256.h"
#include <list>
#include <vector>

class Caterpillar {
public:
    // will return a pointer-like structure later
    Block* GetBlock(const uint256 bhash) const;
    // flush blocks to db + file storage system
    void Store(const std::vector<Block*> vblock);
    // Submits tasks to a single thread in which it topological sorts the blocks
    // and then check their syntax. If all block pass the checking, add them
    // to the corresponding chain in dag_manager.
    void AddNewBlocks(const std::list<const Block> graph);

private:
    std::list<Block&> TopologicalSort(const std::list<const Block&> graph);
};

struct BlockIndex {
    // Only one of ptr_block and file_descriptor (together with offset)
    // should be assigned a value
    Block* ptr_block;
    int file_descriptor;
    uint32_t offset; // offset in file
};

#endif // __SRC_CATERPILLAR_H__
