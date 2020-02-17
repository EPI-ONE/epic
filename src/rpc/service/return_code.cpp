// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "return_code.h"

#include <map>

// in the future, we can add multi language support here
std::string GetReturnStr(RPCReturn code) {
    static const auto codeMap = std::map<RPCReturn, std::string>{
        {RPCReturn::kWalletNotStarted, "Wallet has not been started"},
        {RPCReturn::kWalletEncrypted, "Wallet has already be encrypted with a passphrase"},
        {RPCReturn::kWalletNoPhrase, "Wallet has no phrase set. Please set one first"},
        {RPCReturn::kWalletPhraseSetFailed, "Failed to set passphrase"},
        {RPCReturn::kWalletLoginFailed, "Failed to login with the passphrase. Please check passphrase"},
        {RPCReturn::kWalletPhraseChangeFailed, "Failed to change passphrase. Please check passphrase"},
        {RPCReturn::kWalletNotStarted, "Wallet has not been started"},
        {RPCReturn::kWalletNotLoggedIn, "Please log in or set up a new passphrase"},
        {RPCReturn::kWalletLoggedIn, "You are already logged in"},
        {RPCReturn::kWalletPhraseSet, "Your passphrase has been successfully set!"},
        {RPCReturn::kWalletPhraseUpdated, "Your passphrase is successfully updated!"},

        {RPCReturn::kMinerNotRunning, "Miner is not running yet"},
        {RPCReturn::kMinerStopFailed, "Failed to stop miner"},
        {RPCReturn::kMinerStop, "Miner is successfully stopped"},

        {RPCReturn::kTxNoOutput, "Please specify at least one output"},
        {RPCReturn::kTxWrongAddr, "Invalid address"},
        {RPCReturn::kTxCreateTxFailed, "Failed to create tx. Please check if you have enough balance."},
        {RPCReturn::kTxCreatedSuc, "Now wallet is creating tx"},

        {RPCReturn::kRedeemExceed, "Value exceeding the maximum that can be redeemed."},
        {RPCReturn::kRedeemPending, "A previous redemption is pending. Abort the current one."},
        {RPCReturn::kRedeemSuc, "Successfully redeemed"},

        {RPCReturn::kFirstRegInvalid, "Failed to create the first registration with invalid address"},
        {RPCReturn::kFirstRegSuc, "Successfully created the first registration"},
        {RPCReturn::kFirstRegExist, "The first registration existed"},
        {RPCReturn::kGenerateKeySuc, "Successfully generated new key"},
        {RPCReturn::kGetBalanceSuc, "Successfully get balance"},
        {RPCReturn::kGetWalletAddrsSuc, "Successfully get all the wallet addresses"},
        {RPCReturn::kGetTxOutNotFound, "Target tx out not found"},
        {RPCReturn::kGetTxOutSuc, "Successfully get the transaction output"},
        {RPCReturn::kGetAllTxOutSuc, "Successfully get all the transaction outputs"},
    };

    auto result = codeMap.find(code);
    if (result != codeMap.end()) {
        return result->second;
    } else {
        return "Return code not found";
    }
}

std::string GetReturnStr(uint32_t code) {
    return GetReturnStr(static_cast<RPCReturn>(code));
}
