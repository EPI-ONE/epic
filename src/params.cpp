#include "params.h"

const Params& TestNetParams::GetParams() {
    static const TestNetParams instance;
    return instance;
}

TestNetParams::TestNetParams() {
    targetTimespan   = TARGET_TIMESPAN;
    timeInterval     = TIME_INTERVAL;
    interval         = INTERVAL;
    targetTPS        = 100;
    punctualityThred = PUNTUALITY_THRESHOLD;
    maxTarget        = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney         = MAX_MONEY;
    reward           = 1;
}
