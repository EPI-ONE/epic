// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_HD_CHAIN_H
#define EPIC_HD_CHAIN_H

#include "big_uint.h"
#include "extended_key.h"

#include <memory>

class HDChain {
public:
    void SetMaster(CExtKey&& master) {
        master_ = std::make_unique<CExtKey>(std::move(master));
    }

    void SetNull() {
        master_.reset(nullptr);
    }
    bool IsNull() {
        return master_.get() == nullptr;
    }

    CExtKey GetKey(const std::vector<uint32_t>& keypath);
    CExtPubKey GetPubKey(const std::vector<uint32_t>& keypath);

private:
    std::unique_ptr<CExtKey> master_;
};

#endif // EPIC_HD_CHAIN_H
