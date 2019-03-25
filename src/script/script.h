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
        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return bytes;
        }
};

namespace std {
    // Script
    template<>
    struct tuple_size<Script> : std::integral_constant<std::size_t, 1> {};
  
    template<std::size_t N>
    struct tuple_element<N, Script> {
        using type = decltype(std::declval<Script>().get<N>());
    };
}
