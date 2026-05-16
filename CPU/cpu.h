/**
 * @file cpu.h
 * @author ZJX Marco
 * @date 2026/5/9
 */

#pragma once
#include <cstddef>
#include "../types.h"
#include "../Memory/memory.h"
#include "Cache/cache.h"
#include "Pipeline/pipeline.h"
#include "Register/register.h"

class CPU;

class CPU {
public:
    CPU(CacheInterface* cache, MemoryInterface* memory) : cache(cache), memory(memory) {
        reset();
    };

    void reset();
    void tick();
    void run();
    void setProgram(std::vector<Instruction>& program, u32 init_pc);

private:

    CacheInterface* cache;
    MemoryInterface* memory;

    u32 current_pc;
    u32 next_pc;

    std::vector<Instruction> program;

    Pipeline current_pipeline;
    Pipeline next_pipeline;

    RegisterFile current_register;
    RegisterFile next_register;

    bool halted;
    bool trace_enabled;

    u64 cycles;

    enum class ForwardSource {
        NONE,
        EX_MEM,
        MEM_WB,
    };

    struct ForwardDecision {
        ForwardSource rs1 = ForwardSource::NONE;
        ForwardSource rs2 = ForwardSource::NONE;
    };

    void fetch_stage();

    void decode_stage();

    void execute_stage();

    void memory_stage();

    void writeback_stage();

    void commit();


    bool uses_rs1(RISCV32I op) const;
    bool uses_rs2(RISCV32I op) const;

    bool writes_register(bool valid, const ControlSignals& ctrl, u8 rd) const;
    bool can_forward_from_ex_mem(const EX_MEM& ex_mem) const;
    bool can_forward_from_mem_wb(const MEM_WB& mem_wb) const;

    bool should_stall_for_load_use(const Instruction& inst) const;

    ForwardSource choose_forward_source(u8 src_reg) const;
    ForwardDecision compute_forwarding(const ID_EX& id_ex) const;

    u32 resolve_forwarded_operand(ForwardSource source, u32 original_value) const;

};
