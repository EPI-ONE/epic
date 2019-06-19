#include "params.h"
#include "block.h"
#include "consensus.h"

const Block& Params::GetGenesis() const {
    return *genesis_;
}

const NodeRecord& Params::GetGenesisRecord() const {
    return *genesisRecord_;
}

void Params::CreateGenesis(const std::string& genesisHexStr) {

    const std::vector<unsigned char> parsed = ParseHex(genesisHexStr);
    VStream vs(parsed);
    Block genesisBlock{vs};
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    genesis_       = std::make_unique<Block>(genesisBlock);
    genesisRecord_ = std::make_unique<NodeRecord>(genesisBlock);

    arith_uint256 msTarget    = initialMsTarget * 2 / arith_uint256{targetTimespan};
    arith_uint256 blockTarget = msTarget * arith_uint256{targetTPS} * arith_uint256{timeInterval};
    uint64_t hashRate         = (arith_uint256{maxTarget} / (msTarget + 1)).GetLow64() / timeInterval;

    static auto genesisState = std::make_shared<ChainState>(0, arith_uint256{0}, genesisBlock.GetTime(), msTarget,
        blockTarget, hashRate, std::vector<uint256>{genesisBlock.GetHash()});

    genesisRecord_->LinkChainState(genesisState);
}

unsigned char Params::GetKeyPrefix(KeyPrefixType type) const {
    return keyPrefixes[type];
}

MainNetParams::MainNetParams() {
    targetTimespan       = TARGET_TIMESPAN;
    timeInterval         = TIME_INTERVAL;
    interval             = INTERVAL;
    targetTPS            = TPS;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };
}

TestNetParams::TestNetParams() {
    targetTimespan       = 100;
    timeInterval         = TIME_INTERVAL;
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "01000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85538cc455c00000000ffff"
        "001d41dd157c0101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d010445"
        "4974206973206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20"
        "776f726b696e672101420000000000000000142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);
}

UnitTestParams::UnitTestParams() {
    targetTimespan       = 100;
    timeInterval         = TIME_INTERVAL;
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(1);
    sortitionThreshold   = 1;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "01000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85538cc455c00000000ffff"
        "001d41dd157c0101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d010445"
        "4974206973206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20"
        "776f726b696e672101420000000000000000142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);

    genesisRecord_->snapshot->hashRate = 1;
}

static std::unique_ptr<const Params> pparams;

const Params& GetParams() {
    assert(pparams);
    return *pparams;
}

void SelectParams(ParamsType type) {
    if (type == ParamsType::MAINNET) {
        pparams = std::make_unique<MainNetParams>();
    } else if (type == ParamsType::TESTNET) {
        pparams = std::make_unique<TestNetParams>();
    } else if (type == ParamsType::UNITTEST) {
        pparams = std::make_unique<UnitTestParams>();
    } else {
        throw std::invalid_argument("Invalid Param Type!");
    }

    GENESIS = pparams->GetGenesis();
    GENESIS_RECORD = pparams->GetGenesisRecord();
}
