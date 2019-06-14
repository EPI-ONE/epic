#ifndef __TASM_H__
#define __TASM_H__

#include <array>
#include <functional>
#include <vector>

#include "opcodes.h"
#include "stream.h"
#include "utilstrencodings.h"

typedef std::function<size_t(VStream& data, std::size_t ip)> instruction;

class Tasm {
public:
    class Listing {
    public:
        std::vector<uint8_t> program;
        VStream data;

        Listing() = default;

        Listing(const std::vector<uint8_t>& p, const VStream& d) : program(p), data(d.cbegin(), d.cend()) {}
        Listing(std::vector<uint8_t>&& p, const VStream& d) : program(p), data(d.cbegin(), d.cend()) {}
        Listing(const VStream& d) : data(d.cbegin(), d.cend()) {}

        ADD_SERIALIZE_METHODS;
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
            listing.data += b.data;
            if (!b.program.empty()) {
                for (const auto& prog : b.program) {
                    listing.program.emplace_back(prog);
                }
            }
            return listing;
        }
    };

    explicit Tasm(std::array<instruction, 256> is) {
        is_ = is;
    };

    instruction YieldInstruction(const std::vector<uint8_t> program) {
        return YieldInstructionNChannel(Preprocessor(program));
    }

    std::vector<uint8_t> Preprocessor(const std::vector<uint8_t>& program_) {
        std::vector<uint8_t> program = program_;
        if (program.back() != SUCCESS) {
            program.push_back(FAIL);
            program.push_back(SUCCESS);
        }

        return program;
    }

    int ExecListing(Listing l) {
        return YieldInstruction(l.program)(l.data, 0);
    }

    void SetOp(uint8_t ip, instruction i) {
        is_[ip] = i;
    }

    void CompileSetOp(uint8_t ip, std::vector<uint8_t> op) {
        is_[ip] = YieldInstruction(op);
    }

private:
    std::array<instruction, 256> is_;

    instruction YieldInstructionNChannel(const std::vector<uint8_t> program) {
        return [=](VStream& data, std::size_t ip) {
            std::size_t ip_p = ip;
            uint8_t op;

            do {
                op   = program[ip_p];
                ip_p = is_[op](data, ip_p);
            } while (op != FAIL && op != SUCCESS);

            return op;
        };
    }
};

namespace std {
string to_string(const Tasm::Listing& listing);
}

#endif
