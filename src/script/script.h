#ifndef __SRC_SCRIPT_SCRIPT_H__
#define __SRC_SCRIPT_SCRIPT_H__

#include <vector>

#include "stream.h"
#include "utils/serialize.h"
#include "utilstrencodings.h"

typedef VStream Bytes;

class Script {
public:
    Bytes bytes;

    Script() : bytes(Bytes()) {}
    Script(const Bytes fromBytes) : bytes(fromBytes) {}
    Script(const std::vector<unsigned char>& scriptBytes) : bytes(VStream(scriptBytes)) {}

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
    void Unserialize(Stream& s) {
        size_t size;
        ::Unserialize(s, VARINT(size));
        s.read((char*) bytes.data(), size * sizeof(char));
    }

    std::string ToString() const {
        return HexStr(bytes);
    }
};
#endif /* ifndef __SRC_SCRIPT_SCRIPT_H__ */
