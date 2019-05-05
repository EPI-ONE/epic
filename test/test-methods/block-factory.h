#ifndef __TEST_TEST_METHODS_BLOCK_FACTORY_H__
#define __TEST_TEST_METHODS_BLOCK_FACTORY_H__

#include "block.h"

Block FakeBlock(int numTxInput = 0, int numTxOutput = 0, bool db = false, bool solve = false);
BlockPtr FakeBlockPtr(int numTxInput = 0, int numTxOutput = 0, bool db = false, bool solve = false);
Transaction FakeTx(int numTxInput, int numTxOutput);

#endif /* ifndef __TEST_TEST-METHODS_BLOCK-FACTORY_H__ */
