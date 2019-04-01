// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include "key.h"
#include "pubkey.h"
#include "sync.h"

/** An interface to be implemented by keystores that support signing. */
class SigningProvider {
   public:
    virtual ~SigningProvider() {}
    virtual bool GetPubKey(const CKeyID& address, CPubKey& pubkey) const {
        return false;
    }
    virtual bool GetKey(const CKeyID& address, CKey& key) const {
        return false;
    }
};


/** A virtual base class for key stores */
class CKeyStore : public SigningProvider {
   public:
    //! Add a key to the store.
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey) = 0;

    //! Check whether a key corresponding to a given address is present in the
    //! store.
    virtual bool HaveKey(const CKeyID& address) const = 0;
    virtual std::set<CKeyID> GetKeys() const          = 0;
};

/** Basic key store, that keeps keys in an address->secret map */
class CBasicKeyStore : public CKeyStore {
   protected:
    mutable CCriticalSection cs_KeyStore;

    using KeyMap = std::map<CKeyID, CKey>;
    using WatchKeyMap = std::map<CKeyID, CPubKey>;

    KeyMap mapKeys GUARDED_BY(cs_KeyStore);
    WatchKeyMap mapWatchKeys GUARDED_BY(cs_KeyStore);

   public:
    bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const override;
    bool GetKey(const CKeyID& address, CKey& keyOut) const override;

    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey) override;
    bool AddKey(const CKey& key) { return AddKeyPubKey(key, key.GetPubKey()); }
    bool HaveKey(const CKeyID& address) const override;
    std::set<CKeyID> GetKeys() const override;
};

/** Checks if a CKey is in the given CKeyStore compressed or otherwise*/
bool HaveKey(const CKeyStore& store, const CKey& key);

#endif // BITCOIN_KEYSTORE_H
