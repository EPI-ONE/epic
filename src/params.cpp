#include "params.h"
#include "block.h"
#include "consensus.h"

//extern Params& params;
//Block GENESIS;
//NodeRecord GENESIS_RECORD;

const Block& Params::GetGenesis() const {
    return *genesis_;
}

const NodeRecord& Params::GetGenesisRecord() const {
    return *genesisRecord_;
}

void Params::CreateGenesis() {

    //Block genesisBlock{GENESIS_BLOCK_VERSION};
    //Transaction tx;

    // Construct a script containing the difficulty bits
    // and the following message:
    //std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       //"4656e20696e20746865206576656e696e6720616e64207765206172"
                       //"65207374696c6c20776f726b696e6721");

    std::string genesisHexStr{
        "01000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41"
        "e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85538cc455c00000000ffff"
        "001d41dd157c0101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffff00484704ffff001d010445"
        "4974206973206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20"
        "776f726b696e672101420000000000000000142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    // Convert the string to bytes
    // auto vs = VStream(ParseHex(hexStr));

    const std::vector<unsigned char> parsed = ParseHex(genesisHexStr);
    VStream vs(parsed);
    BlockNet genesisBlock{vs};
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();
    //vs >> genesisBlock;

    // Add input and output
    //tx.AddInput(TxInput(Tasm::Listing(vs)));

    //std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    //tx.AddOutput(TxOutput(66, Tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    //genesisBlock.AddTransaction(tx);
    //genesisBlock.SetDifficultyTarget(0x1d00ffffL);
    //genesisBlock.SetTime(1548078136L);
    //genesisBlock.SetNonce(2081807681);
    //genesisBlock.FinalizeHash();
    //genesisBlock.CalculateOptimalEncodingSize();

    // The following commented lines were used for mining a genesis block
    // int numThreads = 44;
    // ThreadPool solverPool(numThreads);
    // solverPool.Start();
    // Miner m;
    // m.Solve(genesisBlock, solverPool);
    // solverPool.Stop();
    // std::cout << std::to_string(genesisBlock) << std::endl;

    genesis_       = std::make_unique<Block>(genesisBlock);
    genesisRecord_ = std::make_unique<NodeRecord>(BlockNet{genesisBlock});

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

const Params& TestNetParams::GetParams() {
    static const TestNetParams instance;
    return instance;
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

    CreateGenesis();
}

const Params& UnitTestParams::GetParams() {
    static const UnitTestParams instance;
    return instance;
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

    //CreateGenesis();
}

static std::unique_ptr<const Params> pparams;

const Params& GetParams() {
    assert(pparams);
    return *pparams;
}

void SelectParams(ParamsType type)  {
    if (type == ParamsType::MAINNET) {
    } else if (type == ParamsType::TESTNET) {
        pparams = std::make_unique<TestNetParams>();
    } else if (type == ParamsType::UNITTEST) {
        //params = UnitTestParams::GetParams();
    } else {
        // TODO: error handling
    }

    GENESIS = pparams->GetGenesis();
    GENESIS_RECORD = pparams->GetGenesisRecord();
}  
