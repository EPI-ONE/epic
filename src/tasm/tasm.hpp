#ifndef __TASM_H__
#define __TASM_H__

#include <array>
#include <vector>

#include "opcodes.h"
#include "utils/serialize.h"

typedef std::function<size_t(VStream& data, std::size_t ip)> instruction;

class tasm {
public:
    class listing {
    public:
        std::vector<uint8_t> program;
        VStream data;

        listing() = default;

        listing(std::vector<uint8_t>& p, VStream& d) {
            program = p;
            data    = d;
        };

        listing(VStream& d) {
            data = d;
        }

        listing(VStream&& d) {
            data = d;
        }

        ADD_SERIALIZE_METHODS;
        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(program);
            READWRITE(data);
        }

        friend bool operator==(const listing& a, const listing& b) {
            return a.program == b.program && a.data == b.data;
        }
    };

    explicit tasm(std::array<instruction, 256> is) {
        is_ = is;
    };

    instruction yield_instruction(const std::vector<uint8_t> program) {
        return yield_instruction_n_channel(preprocessor(program));
    }

    instruction yield_debug_instruction(const std::vector<uint8_t> program) {
        return yield_instruction_m_channel(preprocessor(program), {SUCCESS}, {SUCCESS});
    }

    std::vector<uint8_t> preprocessor(const std::vector<uint8_t>& program_) {
        std::vector<uint8_t> program = program_;
        if (program.back() != SUCCESS) {
            program.push_back(FAIL);
            program.push_back(SUCCESS);
        }

        return program;
    }

    int exec_listing(listing l) {
        return yield_instruction(l.program)(l.data, 0);
    }

    int exec_debug_listing(listing l) {
        return yield_debug_instruction(l.program)(l.data, 0);
    }

    void set_op(uint8_t ip, instruction i) {
        is_[ip] = i;
    }

    void compile_set_op(uint8_t ip, std::vector<uint8_t> op, bool debug = false) {
        is_[ip] = debug ? yield_debug_instruction(op) : yield_instruction(op);
    }

private:
    std::array<instruction, 256> is_;

    instruction yield_instruction_n_channel(const std::vector<uint8_t> program) {
        return [=](VStream& data, std::size_t ip) {
            std::size_t ip_p = 0;
            uint8_t op;

            do {
                op   = program[ip_p];
                ip_p = is_[op](data, ip_p);
            } while (op != FAIL && op != SUCCESS);

            return op;
        };
    }

    instruction yield_instruction_m_channel(const std::vector<uint8_t>& program,
        const std::vector<uint8_t>& uchannel,
        const std::vector<uint8_t>& lchannel) {
        instruction iuc = yield_instruction_n_channel(uchannel);
        instruction luc = yield_instruction_n_channel(lchannel);

        return [=](VStream& data, std::size_t ip) {
            std::size_t ip_p = 0;
            uint8_t op;

            do {
                iuc(data, 0);
                op   = program[ip_p];
                ip_p = is_[op](data, ip_p);
                luc(data, 0);
            } while (op != 0);

            return ip + 1;
        };
    }
};

#endif
