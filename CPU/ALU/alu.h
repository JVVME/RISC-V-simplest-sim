/**
 * @file alu.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once

#include "../../types.h"

enum class ALUOp;

word_t ALU(ALUOp op, word_t a, word_t b);


enum class ALUOp {
    NOP,
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    SLT,
    SLTU,
    SLL,
    SRL,
    SRA,
};