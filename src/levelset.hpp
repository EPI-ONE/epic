#ifndef __SRC_LEVELSET_H__
#define __SRC_LEVELSET_H__

#include <vector>
#include <string>
#include <memory>
#include <utility>

class Block;
class Levelset {
    public:
        Levelset();
        static std::shared_ptr<Block> Get(const uint32_t height, const int offset);
        void Add(const std::shared_ptr<const Block> pblock);
        void WriteToFile();
    private:
        std::vector<std::pair<Block, int> > vblockstore_;
        std::string filename;
        std::ifstream& in_;
        std::ofstream& on_;
};

#endif //__SRC_LEVELSET_H__
