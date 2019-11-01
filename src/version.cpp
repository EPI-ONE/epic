// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "version.h"
#include "version_config.h"

std::string GetVersionNum() {
    return EPIC_VERSION;
}

std::string GetVersionTimestamp() {
    return BUILD_TIMESTAMP;
}

std::string GetCommitHash() {
    return GIT_COMMIT;
}
