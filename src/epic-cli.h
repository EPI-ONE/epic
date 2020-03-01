// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_EPIC_CLI_H
#define EPIC_EPIC_CLI_H

#include "cli.h"
#include "clilocalsession.h"
#include "rpc_client.h"

using namespace cli;

class EpicCli {
public:
    EpicCli(std::string name);
    ~EpicCli() {
        rpc_.reset();
        cli_.reset();
        session_.reset();
    }
    std::unique_ptr<Menu> CreateSubMenu(const std::string& title);
    std::unique_ptr<Menu> CreateRootMenu();

    void Start();
    void Open(std::ostream&, const std::string&);
    void Close(std::ostream&);
    void Status(std::ostream&);
    void StartMiner(std::ostream&);
    void StopMiner(std::ostream&);
    void GenerateNewKey(std::ostream&);
    void CreateFirstReg(std::ostream&, std::string&);
    void Redeem(std::ostream&, std::string&, std::string&);
    void SetPassphrase(std::ostream&);
    void ChangePassphrase(std::ostream&);
    void Login(std::ostream&);
    void GetBalance(std::ostream&);
    void Connect(std::ostream&, std::string&);
    void Disconnect(std::ostream&, std::string&);
    void CreateRandomTx(std::ostream&, uint32_t);
    void CreateTx(std::ostream&, uint64_t, std::string&);
    void ShowPeer(std::ostream&, std::string&);
    void NetStat(std::ostream&);

private:
    void TryToMine(std::ostream& out);
    std::string InputPassphrase(std::ostream& out);
    std::string GetLine();
    std::optional<std::string> GetNewPassphrase(std::ostream& out);
    std::unique_ptr<RPCClient> rpc_;
    std::unique_ptr<Cli> cli_;
    std::unique_ptr<CliLocalTerminalSession> session_;
    CmdHandler host_menu_;

    std::atomic_bool exit_;
    std::string name_;
};
#endif // EPIC_EPIC_CLI_H
