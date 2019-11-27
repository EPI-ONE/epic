// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_RPC_RETURN_CODE
#define EPIC_RPC_RETURN_CODE

#include <string>

enum RPCReturn : unsigned int {
    kWalletNotStarted = 1,
    kWalletNotLoggedIn,
    kWalletNoPhrase,
    kWalletPhraseSetFailed,
    kWalletEncrypted,
    kWalletLoginFailed,
    kWalletPhraseChangeFailed,

    kWalletLoggedIn,
    kWalletPhraseSet,
    kWalletPhraseUpdated,

    kTxNoOutput,
    kTxWrongAddr,
    kTxCreateTxFailed,
    kTxCreatedSuc,

    kRedeemExceed,
    kRedeemPending,
    kRedeemSuc,

    kFirstRegInvalid,
    kFirstRegSuc,

    kMinerNotRunning,
    kMinerStopFailed,
    kMinerStop,

    kGenerateKeySuc,
    kGetBalanceSuc,

    kCodeNum
};

std::string GetReturnStr(RPCReturn code);
std::string GetReturnStr(uint32_t code);

#endif // EPIC_RPC_RETURN_CODE
