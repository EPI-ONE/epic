// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_KEY_CHAIN_H
#define EPIC_KEY_CHAIN_H

#include "pubkey.h"
#include "wallet_store.h"

#include <optional>

class SecureByte;

class KeyChain {
    virtual ~KeyChain() = default;

    virtual bool HaveKey(const CKeyID&) = 0;

    virtual std::optional<CPubKey> GetPubKey(const CKeyID&) = 0;

    // virtual std::optional<CKey> GetKey(const CKeyID&) = 0;

    virtual void AddKey(const CKeyID&, const CiphertextKey&, const CPubKey&) = 0;

    virtual std::vector<unsigned char> Sign(const CKeyID& addr, const SecureByte& masterEnc, const uint256& hashMsg) = 0;

    virtual size_t GetSize() = 0;
};
#endif // EPIC_KEY_CHAIN_H
