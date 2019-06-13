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
    auto chainwork            = maxTarget / (arith_uint256().SetCompact(genesisBlock.GetDifficultyTarget()) + 1);

    static auto genesisState = std::make_shared<ChainState>(0, chainwork, genesisBlock.GetTime(), msTarget, blockTarget,
                                                            hashRate, std::vector<uint256>{genesisBlock.GetHash()});

    genesisRecord_->LinkChainState(genesisState);
}

unsigned char Params::GetKeyPrefix(KeyPrefixType type) const {
    return keyPrefixes[type];
}

MainNetParams::MainNetParams() {
    version              = GENESIS_BLOCK_VERSION;
    targetTimespan       = TARGET_TIMESPAN;
    timeInterval         = TIME_INTERVAL;
    interval             = INTERVAL;
    targetTPS            = TPS;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    msRewardCoefficient  = REWARD_COEFFICIENT;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "01000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855a2471a5dffff001d8f7f"
        "6c650101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d01044549742069"
        "73206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f726b"
        "696e6721014200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);
}

TestNetParams::TestNetParams() {
    version              = 10;
    targetTimespan       = 100;
    timeInterval         = TIME_INTERVAL;
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    msRewardCoefficient  = REWARD_COEFFICIENT;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "0a000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855388ff95cffff001e3634"
        "c8010101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d01044549742069"
        "73206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f726b"
        "696e6721014200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);
    }

UnitTestParams::UnitTestParams() {
    version              = 100;
    targetTimespan       = 9;
    timeInterval         = 3;
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    msRewardCoefficient  = 1;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(1);
    sortitionThreshold   = 2;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "64000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855388ff95cffff001fb7d5"
        "03000101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d01044549742069"
        "73206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f726b"
        "696e6721014200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);

    genesisRecord_->snapshot->hashRate    = 2;
    genesisRecord_->snapshot->blockTarget = maxTarget;
    genesisRecord_->snapshot->milestoneTarget.SetCompact(0x20c0ffffL);
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

    GENESIS        = pparams->GetGenesis();
    GENESIS_RECORD = pparams->GetGenesisRecord();
}
