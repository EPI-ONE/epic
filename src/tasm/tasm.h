// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_TASM_H
#define EPIC_TASM_H

#include "stream.h"

#include <array>
#include <functional>
#include <vector>

namespace tasm {

using instruction = std::function<size_t(VStream& data, std::size_t ip)>;

class Listing {
public:
    std::vector<uint8_t> program;
    byte_vector data;

    Listing() = default;

    Listing(std::vector<uint8_t> program, const std::vector<uint8_t>& data_)
        : program(std::move(program)), data(data_.cbegin(), data_.cend()) {}

    Listing(const std::vector<uint8_t>& program, const VStream& data_) : program(program) {
        data.resize(data_.size());
        memcpy(&data[0], data_.data(), data_.size());
    }

    Listing(const std::vector<uint8_t>& program, VStream&& data_) : program(program) {
        data_.MoveTo(data);
    }

    explicit Listing(VStream data_) : Listing(std::vector<uint8_t>{}, std::move(data_)) {}

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(program);
        READWRITE(data);
    }

    friend bool operator==(const Listing& a, const Listing& b) {
        return a.program == b.program && a.data == b.data;
    }

    friend Listing operator+(const Listing& a, const Listing& b) {
        Listing listing{a.program, a.data};
        listing.data.insert(listing.data.end(), b.data.begin(), b.data.end());
        if (!b.program.empty()) {
            for (const auto& prog : b.program) {
                listing.program.emplace_back(prog);
            }
        }
        return listing;
    }
};

class Tasm {
public:
    bool Exec(Listing&& l);

private:
    instruction YieldInstructionNChannel(std::vector<uint8_t>&& program);
    std::vector<uint8_t> Preprocess(const std::vector<uint8_t>& program_);

    instruction YieldInstruction(const std::vector<uint8_t>& program) {
        return YieldInstructionNChannel(Preprocess(program));
    }
};

} // namespace tasm

namespace std {
string to_string(const tasm::Listing& listing);
}

#endif // EPIC_TASM_H
