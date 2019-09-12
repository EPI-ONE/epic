// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_HD_CHAIN_H
#define EPIC_HD_CHAIN_H

#include "big_uint.h"

class HDChain {
public:
    void SetSeed();
    void SetNull();
    bool IsNull();
    void DeriveFirstChildKey(); // hardened
    void DeriveNewChildKey();

private:
    uint256 chaincode_;
};

#endif // EPIC_HD_CHAIN_H
