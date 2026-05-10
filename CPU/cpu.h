/**
 * @file cpu.h
 * @author ZJX Marco
 * @date 2026/5/9
 */

#pragma once
#include "../types.h"
#include "../Memory/memory.h"
#include "Pipeline/pipeline.h"
#include "Register/register.h"

class CPU;

class CPU {
public:
    CPU();

    void reset();
    void tick();
    void run();
    void setProgram(std::vector<Instruction>& program, u32 init_pc);

    void setTrace(bool enable);

private:
    u32 current_pc;
    u32 next_pc;

    std::vector<Instruction> program;

    Pipeline current_pipeline;
    Pipeline next_pipeline;

    RegisterFile current_register;
    RegisterFile next_register;

    Memory memory;

    bool halted;

    u64 cycles;

    void fetch_stage();

    void decode_stage();

    void execute_stage();

    void memory_stage();

    void writeback_stage();

    void commit();

};
