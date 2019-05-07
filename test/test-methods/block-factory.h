#ifndef __TEST_TEST_METHODS_BLOCK_FACTORY_H__
#define __TEST_TEST_METHODS_BLOCK_FACTORY_H__

#include "consensus.h"

Block FakeBlock(int numTxInput = 0, int numTxOutput = 0, bool solve = false);
ConstBlockPtr FakeBlockPtr(int numTxInput = 0, int numTxOutput = 0, bool solve = false);
NodeRecord FakeNodeRecord(const BlockNet&);

#endif /* ifndef __TEST_TEST-METHODS_BLOCK-FACTORY_H__ */
