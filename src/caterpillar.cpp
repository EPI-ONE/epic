#include "caterpillar.h"

Caterpillar::Caterpillar(std::string& dbPath) {
    dbStore_ = RocksDBStore(dbPath);
    thread_.Start();
}

bool Caterpillar::AddNewBlock(const BlockPtr blk) {
    if (Exisis(blk->GetHash())) {
        return true;
    }
    return true;
}

Caterpillar::~Caterpillar() {
    thread_.Stop();
    dbStore_.~RocksDBStore();
    obc_.~OrphanBlocksContainer();
}
