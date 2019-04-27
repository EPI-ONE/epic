#ifndef __SRC_SCRIPT_SCRIPT_H__
#define __SRC_SCRIPT_SCRIPT_H__

#include <vector>

#include "stream.h"
#include "utilstrencodings.h"

typedef VStream Bytes;

class Script {
public:
    Bytes bytes;

    Script() : bytes(Bytes()) {}
    Script(const Bytes fromBytes) : bytes(fromBytes) {}
    Script(const std::vector<unsigned char>& scriptBytes) : bytes(VStream(scriptBytes)) {}

    /*
     * TODO: counts the number of OPs for sig verification
     */
    static int GetSigOpCount(Bytes program) {
        return 0;
    }

    void clear() {
        bytes.clear();
    }

    template <typename Stream>
    void Serialize(Stream& s) const {
        ::Serialize(s, VARINT(bytes.size()));
        if (!bytes.empty()) {
            s << bytes;
        }
    }

    template <typename Stream>
    void Deserialize(Stream& s) {
        size_t size;
        ::Deserialize(s, VARINT(size));
        bytes = VStream(size);
        s.read((char*) bytes.data(), size);
    }
};

namespace std {
string to_string(const Script& script);
}

#endif /* ifndef __SRC_SCRIPT_SCRIPT_H__ */
