/**
 * @file pipeline.h
 * @author ZJX Marco
 * @date 2026/5/9
 */

#pragma once
#include "../../types.h"
#include "../../ISA/instruction.h"
#include "../ALU/alu.h"

enum class ALUOp;
struct ControlSignals;
struct PipelineControlSignals;
struct IF_ID;
struct ID_EX;
struct EX_MEM;
struct MEM_WB;
class Pipeline;


struct ControlSignals {
    bool reg_write = false;

    bool mem_read = false;

    bool mem_write = false;

    bool mem_to_reg = false;

    bool branch = false;

    bool jump = false;

    bool alu_src = false;

    ALUOp alu_op = ALUOp::NOP;
};

struct PipelineControlSignals {
    bool stall_if = false;

    bool stall_id = false;

    bool stall_ex = false;

    bool stall_mem = false;

    bool stall_wb = false;
};

struct IF_ID {
    bool valid = false;

    Instruction inst;

};

struct ID_EX {
    bool valid = false;

    u32 rs1_value = 0;
    u32 rs2_value = 0;

    u32 imm = 0;

    Instruction inst;

    ControlSignals ctrl;
};

struct EX_MEM {
    bool valid = false;

    u32 alu_result = 0;

    u32 rs2_value = 0;

    u8 rd = 0;

    Instruction inst;

    ControlSignals ctrl;
};

struct MEM_WB {
    bool valid = false;

    uint32_t writeback_value = 0;

    uint8_t rd = 0;

    Instruction inst;

    ControlSignals ctrl;
};

class Pipeline {
public:
    Pipeline() = default;

    IF_ID if_id;
    ID_EX id_ex;
    EX_MEM ex_mem;
    MEM_WB mem_wb;
};
