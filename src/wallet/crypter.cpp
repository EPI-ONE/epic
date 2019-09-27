// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"
#include "aes.h"

#include <openssl/evp.h>
#include <string>
#include <vector>

bool Crypter::SetKeyFromPassphrase(const SecureString& strKeyData,
                                    const std::vector<unsigned char>& salt,
                                    const unsigned int nRounds) {
    if (nRounds < 1 || salt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    SecureByte out;
    out.resize(64);
    int i = PKCS5_PBKDF2_HMAC_SHA1(strKeyData.data(), strKeyData.size(), salt.data(), salt.size(), nRounds, WALLET_CRYPTO_KEY_SIZE, out.data());

    if (i == 0) {
        memory_cleanse(passphraseKey_.data(), passphraseKey_.size());
        memory_cleanse(passphraseIV_.data(), passphraseIV_.size());
        return false;
    }

    memcpy(passphraseKey_.data(), out.data(), WALLET_CRYPTO_KEY_SIZE);
    memcpy(passphraseIV_.data(), out.data() + WALLET_CRYPTO_KEY_SIZE, WALLET_CRYPTO_IV_SIZE);
    memory_cleanse(out.data(), out.size());

    fKeySet_ = true;
    return true;
}

bool Crypter::SetKey(const SecureByte& chNewKey, const std::vector<unsigned char>& chNewIV) {
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_IV_SIZE)
        return false;

    memcpy(passphraseKey_.data(), chNewKey.data(), chNewKey.size());
    memcpy(passphraseIV_.data(), chNewIV.data(), chNewIV.size());

    fKeySet_ = true;
    return true;
}

bool Crypter::EncryptMaster(std::vector<unsigned char>& ciphertext) const {
    if (!IsReady()) {
        return false;
    }

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCKSIZE bytes
    ciphertext.resize(master_.size() + AES_BLOCKSIZE);

    AES256CBCEncrypt enc(passphraseKey_.data(), passphraseIV_.data(), false);
    size_t nLen = enc.Encrypt(master_.data(), master_.size(), ciphertext.data());
    if (nLen < master_.size()) {
        return false;
    }

    ciphertext.resize(nLen);
    return true;
}

bool Crypter::DecryptMaster(const std::vector<unsigned char> &ciphertext)  {
    if (!(fKeySet_ && !fMaster_)) {
        return false;
    }

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = ciphertext.size();

    master_.resize(nLen);

    AES256CBCDecrypt dec(passphraseKey_.data(), passphraseIV_.data(), false);
    nLen = dec.Decrypt(ciphertext.data(), ciphertext.size(), master_.data());
    if (nLen == 0) {
        return false;
    }
    master_.resize(nLen);

    fMaster_ = true;
    return true;
}

bool Crypter::EncryptKey(const CPubKey& pubkey, const CKey& key, std::vector<unsigned char>& cryptedPriv) const {
    if (!IsReady()) {
        return false;
    }

    // max cryptedPriv len for a n bytes of plaintext is
    // n + AES_BLOCKSIZE bytes
    cryptedPriv.resize(key.size() + AES_BLOCKSIZE);
    SecureByte secret{key.begin(), key.end()};
    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    auto pubkeyHash = pubkey.GetHash();
    memcpy(chIV.data(), &pubkeyHash, WALLET_CRYPTO_IV_SIZE);

    AES256CBCEncrypt enc(master_.data(), chIV.data(), false);
    size_t nLen = enc.Encrypt(secret.data(), secret.size(), cryptedPriv.data());
    if (nLen < secret.size()) {
        return false;
    }

    cryptedPriv.resize(nLen);
    return true;
}

bool Crypter::DecryptKey(const CPubKey& pubkey, const std::vector<unsigned char>& cryptedPriv, CKey& key) const {
    if (!IsReady()) {
        return false;
    }

    // plaintext(secret) will always be equal to or lesser than length of ciphertext
    int nLen = cryptedPriv.size();
    SecureByte secret;
    secret.resize(nLen);

    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    auto pubkeyHash = pubkey.GetHash();
    memcpy(chIV.data(), &pubkeyHash, WALLET_CRYPTO_IV_SIZE);

    AES256CBCDecrypt dec(master_.data(), chIV.data(), false);
    nLen = dec.Decrypt(cryptedPriv.data(), cryptedPriv.size(), secret.data());
    if (nLen == 0) {
        return false;
    }
    secret.resize(nLen);

    if (secret.size() != 32) {
        return false;
    }

    key.Set(secret.begin(), secret.end(), pubkey.IsCompressed());
    return key.VerifyPubKey(pubkey);
}
