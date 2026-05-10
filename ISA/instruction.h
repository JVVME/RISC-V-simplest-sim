/**
 * @file instruction.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../types.h"

enum class RISCV32I;
enum class InstructionType;
struct R_type_instruction;
struct I_type_instruction;
struct S_type_instruction;
struct B_type_instruction;
struct J_type_instruction;
struct Instruction;

std::vector<Instruction> parse_assembly_file( const std::string &filename);


enum class RISCV32I {
    NOP,
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    ADDI,
    ANDI,
    ORI,
    LW,
    SW,
    BEQ,
    BNE,
    JAL,
    JALR,
};

enum class InstructionType {
    R,
    I,
    S,
    B,
    J,
};

struct R_type_instruction {
    static constexpr auto type = InstructionType::R;
    u8 funct7;
    u8 rs2;
    u8 rs1;
    u8 funct3;
    u8 rd;
    u8 opcode;
};

struct I_type_instruction {
    static constexpr auto type = InstructionType::I;
    u32 imm;
    u8 rs1;
    u8 funct3;
    u8 rd;
    u8 opcode;
};

struct S_type_instruction {
    u8 imm1;
    u8 rs2;
    u8 rs1;
    u8 funct3;
    u8 imm2;
    u8 opcode;
};

struct B_type_instruction {
    static constexpr auto type = InstructionType::B;
    u8 i;
    u8 imm1;
    u8 rs2;
    u8 rs1;
    u8 funct3;
    u8 imm2;
    u8 imm3;
    u8 opcode;
};

struct J_type_instruction {
    static constexpr auto type = InstructionType::J;
    u8 i1;
    u32 imm1;
    u8 i2;
    u8 imm2;
    u8 rd;
    u8 opcode;
};

struct Instruction {
    RISCV32I op = RISCV32I::NOP;

    u8 rd = 0;
    u8 rs1 = 0;
    u8 rs2 = 0;

    i32 imm = 0;

    u32 pc = 0;
};

