// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test_factory.h"

int main(int argc, char* argv[]) {
    std::map<std::string, uint32_t> targets = {
        {"1", 0x2100ffffL}, {"10", 0x2100ffffL}, {"11", 0x2100ffffL}, {"100", 0x2100ffffL}};

    std::string version;
    if (argc == 1) {
        spdlog::info("No params type passed. Using the default: UNITTEST");
        version = "100";
        SelectParams(ParamsType::UNITTEST, false);
    } else {
        version = argv[1];

        // clang-format off
        auto params_type = version == "1" ? ParamsType::MAINNET :
                           version == "10" ? ParamsType::SPADE :
                           version == "11" ? ParamsType::DIAMOND :
                           version == "100" ? ParamsType::UNITTEST :
                           (ParamsType) 66;
        // clang-format on

        try {
            SelectParams(params_type, false);
            std::stringstream targetStr;
            targetStr << std::hex << targets[version];
            spdlog::info("Selected params type: {}. cycleLen = {}, target = 0x{}L", ParamsTypeStr[params_type],
                         GetParams().cycleLen, targetStr.str());
        } catch (const std::invalid_argument& e) {
            spdlog::error(
                "{} Please input a valid version number: \n   '1' (MAINNET), '10' (SPADE), '11' (DIAMOND), or "
                "'100' (UNITTEST). ",
                e.what());
            return 66;
        }
    }

    Transaction tx;

    // Construct a script containing the difficulty bits and the following message:
    std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       "4656e20696e20746865206576656e696e6720616e64207765206172"
                       "65207374696c6c20776f726b696e6721");

    // Convert the string to bytes
    auto vs = VStream(ParseHex(hexStr));

    // Add input and output
    tx.AddInput(TxInput(tasm::Listing(vs)));

    std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    tx.AddOutput(TxOutput(66, tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    Block genesisBlock{GetParams().version};
    genesisBlock.SetDifficultyTarget(targets[version]);
    genesisBlock.SetTime(1559859000L);
    genesisBlock.SetNonce(0);
    genesisBlock.InitProofSize(GetParams().cycleLen);
    genesisBlock.AddTransaction(tx);
    genesisBlock.SetMerkle();
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    int numThreads = std::thread::hardware_concurrency() / 10;
    auto m         = Miner(numThreads);
    m.Start();
    m.Solve(genesisBlock);
    spdlog::info("Mined Genesis\n{}", std::to_string(genesisBlock));
    VStream gvs(genesisBlock);
    spdlog::info("HEX string for version [{}]: \n{}", genesisBlock.GetVersion(), HexStr(gvs.cbegin(), gvs.cend()));
    assert(genesisBlock.Verify());
    m.Stop();

    return 0;

    /****************************** Last mining result *******************************
     MAINNET:
         nonce = 24
         proof = {6251303, 10147014, 13279633, 13986170, 21369580, 46393935, 62080194,
                  73367960, 81955500, 88066012, 130686755, 143620100, 144917087, 155171399,
                  156073124, 158766522, 160587723, 180465357, 229417443, 254360015, 265707652,
                  275375962, 288281853, 297815130, 310645549, 328132943, 347080859, 347600807,
                  349758923, 370329272, 375881440, 410271724, 411860289, 415421890, 425097674,
                  465253991, 471572753, 480185136, 480539041, 482914708, 495008315, 523604902}

     SPADE:
         nonce = 2
         proof = {110002832, 187793670, 201366278, 448514938}

     DIAMOND:
        nonce = 5
        proof = { }

     UNITTEST:
         nonce = 0
         proof = { }
    *********************************************************************************/
}
