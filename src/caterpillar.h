#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <vector>

#include <block.h>
#include <uint256.h>

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

#endif // __SRC_CATERPILLAR_H__
