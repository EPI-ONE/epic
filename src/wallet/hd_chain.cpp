// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hd_chain.h"

CExtKey HDChain::GetKey(const std::vector<uint32_t>& keypath) {
    CExtKey key = *master_;

    for (const auto& nChild: keypath) {
        CExtKey newkey;
        key.Derive(newkey, nChild);
    }
    return CExtKey();
}

CExtPubKey HDChain::GetPubKey(const std::vector<uint32_t>& keypath) {
    uint32_t lastHarden = 0;
    for (size_t i = keypath.size() - 1; i >= 0; i--) {
        if (keypath[i] & 0x80000000) {
            lastHarden = i;
            break;
        }
    }

    CExtKey key = *master_;
    for (size_t i = 0; i <= lastHarden; i++) {
        CExtKey newkey;
        key.Derive(newkey, keypath[i]);
    }
    
    CExtPubKey pubkey = key.Neuter();
    if (lastHarden == keypath.size() - 1) {
        return pubkey;
    } 

    // use direct pubkey derivation to reduce exposure of prvkey
    for (size_t i=lastHarden+1; i<keypath.size(); i++){
        CExtPubKey newpubkey;
        pubkey.Derive(newpubkey, keypath[i]);
        pubkey = newpubkey;
    }

    return pubkey;
}
