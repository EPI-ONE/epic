#include "utils/serialize.h"
#include "utilstrencodings.h"
#include <vector>

typedef std::vector<unsigned char> Bytes;

class Script {
public:
    Bytes bytes;

    Script() : bytes(Bytes()) {
    }
    Script(const Bytes fromBytes) : bytes(fromBytes) {
    }

    void clear() {
        bytes.clear();
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(bytes);
    }

    std::string ToString() const {
        std::string str;
        str += HexStr(bytes);
        return str;
    }
};
