// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tasm.h"
#include "functors.h"
#include "opcodes.h"
#include "utilstrencodings.h"

namespace tasm {

std::vector<uint8_t> Tasm::Preprocess(const std::vector<uint8_t>& program_) {
    std::vector<uint8_t> program{};
    for (const auto& opcode : program_) {
        program.emplace_back(opcode);
        if (opcode != FAIL || opcode != SUCCESS) {
            program.emplace_back(FAIL);
        }
    }

    if (program.back() != SUCCESS) {
        program.push_back(SUCCESS);
    }

    return program;
}

bool Tasm::Exec(Listing&& l) {
    VStream vs(std::move(l.data));
    return bool(YieldInstruction(l.program)(vs, 0));
}

instruction Tasm::YieldInstructionNChannel(std::vector<uint8_t>&& program) {
    return [program = std::move(program)](VStream& vdata, std::size_t instr_ptr) {
        uint8_t op = program[instr_ptr];

        while (op != FAIL && op != SUCCESS) {
            instr_ptr = functors[op](vdata, instr_ptr);
            op        = program[instr_ptr];
        }
        return op;
    };
}

} // namespace tasm

std::string std::to_string(const tasm::Listing& listing) {
    std::string s = "[ ";

    for (const uint8_t& op : listing.program) {
        s += std::to_string(op);
        s += " ";
    }

    s += "]";
    s += "( " + HexStr(listing.data) + " )";
    return s;
}
