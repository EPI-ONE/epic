// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include "key.h"
#include "secure.h"
#include "serialize.h"

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE  = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE   = 16;

/**
 * Private key encryption is done based on a MasterInfo,
 * which holds a salt and random encryption key generated
 * from mnemonics.
 *
 * MasterInfo are encrypted using AES-256-CBC using a key
 * derived using derivation method EVP_sha512() and
 * derivation iterations nDeriveIterations.
 *
 * Wallet Private Keys are then encrypted using AES-256-CBC
 * with the double-sha256 of the public key as the IV, and the
 * master key's key as the encryption key.
 */

/**
 * Master key information for wallet encryption
 * The number of rounds of derive iterations is estimated to be about 0.1 second
 * to avoid brute force attack
 */
class MasterInfo {
public:
    MasterInfo() : cryptedMaster(WALLET_CRYPTO_KEY_SIZE), salt(WALLET_CRYPTO_SALT_SIZE), nDeriveIterations(25000) {}

    std::vector<unsigned char> cryptedMaster;
    std::vector<unsigned char> salt;
    unsigned int nDeriveIterations;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(cryptedMaster);
        READWRITE(salt);
        READWRITE(nDeriveIterations);
    }

    bool IsNull() const {
        unsigned char zeros[WALLET_CRYPTO_KEY_SIZE];
        memset(zeros, 0, WALLET_CRYPTO_KEY_SIZE);
        return memcmp(cryptedMaster.data(), zeros, WALLET_CRYPTO_KEY_SIZE) == 0;
    }
};


/** Encryption/decryption context with key information */
class Crypter {
private:
    SecureByte passphraseKey_;
    SecureByte passphraseIV_;
    SecureByte master_;
    bool fKeySet_;
    bool fMaster_;

    bool SetKey(const SecureByte& chNewKey, const std::vector<unsigned char>& chNewIV);

public:
    bool SetKeyFromPassphrase(const SecureString& strKeyData,
                              const std::vector<unsigned char>& chSalt,
                              const unsigned int nRounds);
    bool EncryptMaster(std::vector<unsigned char>& ciphertext) const;
    bool DecryptMaster(const std::vector<unsigned char>& ciphertext);
    bool EncryptKey(const CPubKey& pubkey, const CKey& key, std::vector<unsigned char>& cryptedPriv) const;
    bool DecryptKey(const CPubKey& pubkey, const std::vector<unsigned char>& cryptedPriv, CKey& key) const;

    bool SetMaster(const SecureByte& master) {
        if (master.size() != 32) {
            return false;
        }
        memcpy(master_.data(), master.data(), master.size());
        fMaster_ = true;
        return true;
    } 

    void CleanKey() {
        memory_cleanse(passphraseKey_.data(), passphraseKey_.size());
        memory_cleanse(passphraseIV_.data(), passphraseIV_.size());
        fKeySet_ = false;
    }

    inline bool IsReady() const {
        return fKeySet_ && fMaster_;
    }

    Crypter() : fKeySet_(false) , fMaster_(false) {
        passphraseKey_.resize(WALLET_CRYPTO_KEY_SIZE);
        passphraseIV_.resize(WALLET_CRYPTO_IV_SIZE);
        master_.resize(WALLET_CRYPTO_KEY_SIZE);
    }

    explicit Crypter(const SecureByte& masterKey) : master_(masterKey), fKeySet_(false), fMaster_(true) {
        passphraseKey_.resize(WALLET_CRYPTO_KEY_SIZE);
        passphraseIV_.resize(WALLET_CRYPTO_IV_SIZE);
        master_.resize(WALLET_CRYPTO_KEY_SIZE);
    }

    ~Crypter() {
        CleanKey();
        memory_cleanse(master_.data(), master_.size());
    }
};

#endif // BITCOIN_WALLET_CRYPTER_H
