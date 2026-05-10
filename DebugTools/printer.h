/**
 * @file printer.h
 * @author ZJX Marco
 * @date 2026/5/8
 */

#pragma once


#include <vector>
#include "../types.h"
#include "../ISA/instruction.h"
#include "../CPU/Pipeline/pipeline.h"
#include "../CPU/Register/register.h"

void print_program(const std::vector<Instruction>& program);

// 每周期打印
void print_cycle_trace(u64 cycle, u32 pc, bool halted, const Pipeline& pipeline, const RegisterFile& regfile);

// 程序结束打印
void print_final_registers(u64 total_cycles, const RegisterFile& regfile);
