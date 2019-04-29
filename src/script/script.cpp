#include "script.h"

std::string std::to_string(const Script& script) {
    return HexStr(script.bytes);
}