#include <vector>
#include "utilstrencodings.h"

typedef std::vector<unsigned char> Bytes;

class Script {
    public:
        Bytes bytes;

        Script() : bytes(Bytes()) {}
        Script(const Bytes fromBytes): bytes(fromBytes) {}

        void clear() {
            bytes.clear();
        }

        std::string ToString() const {
            std::string str;
            str += HexStr(bytes);
            return str;
        }
};
