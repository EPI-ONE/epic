#ifndef EPIC_EXTENDED_KEY_H
#define EPIC_EXTENDED_KEY_H

#include "key.h"

constexpr uint32_t BIP32_EXTKEY_SIZE = 74;

struct CExtPubKey;

struct CExtKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CKey key;

    friend bool operator==(const CExtKey& a, const CExtKey& b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0], sizeof(vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.key == b.key;
    }

    void SetSeed(const unsigned char* seed, unsigned int nSeedLen);
    bool Derive(CExtKey& out, unsigned int nChild) const;
    CExtPubKey Neuter() const;
    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
};

struct CExtPubKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CPubKey pubkey;

    friend bool operator==(const CExtPubKey &a, const CExtPubKey &b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0], sizeof(vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.pubkey == b.pubkey;
    }

    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
    bool Derive(CExtPubKey& out, unsigned int nChild) const;
};

std::string EncodeExtKey(const CExtKey& extkey);
std::optional<CExtKey> DecodeExtKey(const std::string& str);
std::string EncodeExtPubKey(const CExtPubKey& extpubkey);
std::optional<CExtPubKey> DecodeExtPubKey(const std::string& str);

#endif // EPIC_EXTENDED_KEY_H
