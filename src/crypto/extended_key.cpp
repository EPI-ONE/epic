#include "extended_key.h"

#include "base58.h"

void CExtKey::SetSeed(const unsigned char* seed, unsigned int nSeedLen) {
    static const unsigned char hashkey[] = {'e', 'p', 'i', 'c', ' ', 's', 'e', 'e', 'd'};
    std::vector<unsigned char, secure_allocator<unsigned char>> vout(64);
    // use block2b instead of chmac_sha512
    BLAKE2B{vout.size(), hashkey, sizeof(hashkey)}.Write(seed, nSeedLen).Finalize(vout.data());
    key.Set(vout.data(), vout.data() + 32, true);
    memcpy(chaincode.begin(), vout.data() + 32, 32);
    nDepth = 0;
    nChild = 0;
    memset(vchFingerprint, 0, sizeof(vchFingerprint));
}

bool CExtKey::Derive(CExtKey& out, unsigned int _nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id  = key.GetPubKey().GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = _nChild;
    return key.Derive(out.key, out.chaincode, _nChild, chaincode);
}

CExtPubKey CExtKey::Neuter() const {
    CExtPubKey ret;
    ret.nDepth = nDepth;
    memcpy(&ret.vchFingerprint[0], &vchFingerprint[0], 4);
    ret.nChild    = nChild;
    ret.pubkey    = key.GetPubKey();
    ret.chaincode = chaincode;
    return ret;
}

void CExtKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const {
    code[0] = nDepth;
    memcpy(code + 1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF;
    code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >> 8) & 0xFF;
    code[8] = (nChild >> 0) & 0xFF;
    memcpy(code + 9, chaincode.begin(), 32);
    code[41] = 0;
    assert(key.size() == 32);
    memcpy(code + 42, key.begin(), 32);
}

void CExtKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code + 1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code + 9, 32);
    key.Set(code + 42, code + BIP32_EXTKEY_SIZE, true);
}

bool CExtPubKey::Derive(CExtPubKey& out, unsigned int _nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id  = pubkey.GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = _nChild;
    return pubkey.Derive(out.pubkey, out.chaincode, _nChild, chaincode);
}

void CExtPubKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const {
    code[0] = nDepth;
    memcpy(code + 1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF;
    code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >> 8) & 0xFF;
    code[8] = (nChild >> 0) & 0xFF;
    memcpy(code + 9, chaincode.begin(), 32);
    assert(pubkey.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE);
    memcpy(code + 41, pubkey.begin(), CPubKey::COMPRESSED_PUBLIC_KEY_SIZE);
}

void CExtPubKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code + 1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code + 9, 32);
    pubkey.Set(code + 41, code + BIP32_EXTKEY_SIZE);
}

std::string EncodeExtKey(const CExtKey& key) {
    std::vector<unsigned char> data{GetParams().GetKeyPrefix(Params::KeyPrefixType::EXT_SECRET_KEY)};
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::optional<CExtKey> DecodeExtKey(const std::string& str) {
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix{GetParams().GetKeyPrefix(Params::KeyPrefixType::EXT_SECRET_KEY)};
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
        return key;
    }
    return {};
}

std::string EncodeExtPubKey(const CExtPubKey& key) {
    std::vector<unsigned char> data{GetParams().GetKeyPrefix(Params::KeyPrefixType::EXT_PUBLIC_KEY)};
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

std::optional<CExtPubKey> DecodeExtPubKey(const std::string& str) {
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& prefix{GetParams().GetKeyPrefix(Params::KeyPrefixType::EXT_PUBLIC_KEY)};
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
        return key;
    }
    return {};
}
