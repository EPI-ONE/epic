#ifndef __SRC_BUNDLE_H__
#define __SRC_BUNDLE_H__

#include <vector>

#include <block.h>
#include <uint256.h>

class Bundle {
    public:
        std::vector<std::shared_ptr<const Block> > vblocks;
        std::vector<uint256> hashes;
        uint32_t nonce;
        size_t size;
        uint8_t type;

        Bundle();
        Bundle(std::vector<std::shared_ptr<const Block> >&& vblocks):
            vblocks(std::move(vblocks)) {}
        ~Bundle();

        void AddBlock(std::shared_ptr<const Block> pblock);
        // Get the first hash of the block vector which should be the milestone
        // block.
        uint256 GetFirstHash();

        decltype(auto) get() const {
            if constexpr (N == 0) return vblocks;
            else if constexpr (N == 1) return hashes;
            else if constexpr (N == 2) return nonce;
            else if constexpr (N == 3) return size;
            else if constexpr (N == 4) return type;
        }
};

namespace std {
    // Bundle 
    template<>
    struct tuple_size<Bundle> : std::integral_constant<std::size_t, 5> {};

    template<std::size_t N>
    struct tuple_element<N, Bundle> {
        using type = decltype(std::declval<Bundle>().get<N>());
    };
}

#endif // __SRC_BUNDLE_H__
