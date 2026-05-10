//
// Created by zhang on 2026/5/7.
//

#include "instruction.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string trim(const std::string &s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return "";

    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string to_upper(std::string s) {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return std::toupper(c); });
    return s;
}

u8 parse_register(const std::string &reg) {
    if (reg.empty() || reg[0] != 'x')
        throw std::runtime_error("Invalid register: " + reg);

    return static_cast<u8>(std::stoi(reg.substr(1)));
}

i32 parse_imm(const std::string &s) {
    return static_cast<i32>(std::stoi(s, nullptr, 0));
}

RISCV32I parse_opcode(const std::string &op) {
    const auto opcode = to_upper(op);

    if (opcode == "NOP")
        return RISCV32I::NOP;

    if (opcode == "ADD")
        return RISCV32I::ADD;

    if (opcode == "SUB")
        return RISCV32I::SUB;

    if (opcode == "AND")
        return RISCV32I::AND;

    if (opcode == "OR")
        return RISCV32I::OR;

    if (opcode == "XOR")
        return RISCV32I::XOR;

    if (opcode == "ADDI")
        return RISCV32I::ADDI;

    if (opcode == "ANDI")
        return RISCV32I::ANDI;

    if (opcode == "ORI")
        return RISCV32I::ORI;

    if (opcode == "LW")
        return RISCV32I::LW;

    if (opcode == "SW")
        return RISCV32I::SW;

    if (opcode == "BEQ")
        return RISCV32I::BEQ;

    if (opcode == "BNE")
        return RISCV32I::BNE;

    if (opcode == "JAL")
        return RISCV32I::JAL;

    if (opcode == "JALR")
        return RISCV32I::JALR;

    throw std::runtime_error("Unknown opcode: " + op);
}

} // namespace

std::vector<Instruction> parse_assembly_file( const std::string &filename) {

    std::ifstream file(filename);

    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filename);

    std::vector<std::string> lines;

    std::string line;

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    std::unordered_map<std::string, u32> labels;

    u32 pc = 0;

    // PASS 1: collect labels
    for (const auto &raw_line : lines) {

        auto line = raw_line;

        if (const auto pos = line.find('#'); pos != std::string::npos)
            line = line.substr(0, pos);

        line = trim(line);

        if (line.empty())
            continue;

        if (line.back() == ':') {
            const auto label = trim(line.substr(0, line.size() - 1));
            labels[label] = pc;
            continue;
        }

        pc += 4;
    }

    // PASS 2: parse instructions
    std::vector<Instruction> program;

    pc = 0;

    for (const auto &raw_line : lines) {

        auto line = raw_line;

        if (const auto pos = line.find('#'); pos != std::string::npos)
            line = line.substr(0, pos);

        line = trim(line);

        if (line.empty())
            continue;

        if (line.back() == ':')
            continue;

        std::ranges::replace(line, ',', ' ');

        std::stringstream ss(line);

        std::string op_string;

        ss >> op_string;

        Instruction inst;

        inst.op = parse_opcode(op_string);

        inst.pc = pc;

        switch (inst.op) {

        case RISCV32I::NOP:
            break;

        case RISCV32I::ADD:
        case RISCV32I::SUB:
        case RISCV32I::AND:
        case RISCV32I::OR:
        case RISCV32I::XOR: {

            std::string rd, rs1, rs2;

            ss >> rd >> rs1 >> rs2;

            inst.rd = parse_register(rd);
            inst.rs1 = parse_register(rs1);
            inst.rs2 = parse_register(rs2);

            break;
        }

        case RISCV32I::ADDI:
        case RISCV32I::ANDI:
        case RISCV32I::ORI: {

            std::string rd, rs1, imm;

            ss >> rd >> rs1 >> imm;

            inst.rd = parse_register(rd);
            inst.rs1 = parse_register(rs1);
            inst.imm = parse_imm(imm);

            break;
        }

        case RISCV32I::LW: {

            std::string rd, addr;

            ss >> rd >> addr;

            const auto l = addr.find('(');
            const auto r = addr.find(')');

            inst.rd = parse_register(rd);

            inst.imm = parse_imm(addr.substr(0, l));

            inst.rs1 =
                parse_register(addr.substr(l + 1, r - l - 1));

            break;
        }

        case RISCV32I::SW: {

            std::string rs2, addr;

            ss >> rs2 >> addr;

            const auto l = addr.find('(');
            const auto r = addr.find(')');

            inst.rs2 = parse_register(rs2);

            inst.imm = parse_imm(addr.substr(0, l));

            inst.rs1 =
                parse_register(addr.substr(l + 1, r - l - 1));

            break;
        }

        case RISCV32I::BEQ:
        case RISCV32I::BNE: {

            std::string rs1, rs2, label;

            ss >> rs1 >> rs2 >> label;

            inst.rs1 = parse_register(rs1);
            inst.rs2 = parse_register(rs2);

            inst.imm =
                static_cast<i32>(labels[label]) -
                static_cast<i32>(pc);

            break;
        }

        case RISCV32I::JAL: {

            std::string rd, label;

            ss >> rd >> label;

            inst.rd = parse_register(rd);

            inst.imm =
                static_cast<i32>(labels[label]) -
                static_cast<i32>(pc);

            break;
        }

        case RISCV32I::JALR: {

            std::string rd, addr;

            ss >> rd >> addr;

            const auto l = addr.find('(');
            const auto r = addr.find(')');

            inst.rd = parse_register(rd);

            inst.imm = parse_imm(addr.substr(0, l));

            inst.rs1 =
                parse_register(addr.substr(l + 1, r - l - 1));

            break;
        }
        }

        program.push_back(inst);

        pc += 4;
    }

    return program;
}
