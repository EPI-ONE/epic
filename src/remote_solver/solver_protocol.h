// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SOLVER_PROTOCOL_H
#define EPIC_SOLVER_PROTOCOL_H

namespace SolverResult{
    enum ErrorCode{
        SUCCESS = 0,
        TASK_CANCELED_BY_CLIENT,
        INVALID_PARAM,
        SERVER_ABORT,
        UNKNOWN_ERROR
    };
}
#endif // EPIC_SOLVER_PROTOCOL_H
