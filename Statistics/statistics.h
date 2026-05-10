/**
 * @file statistics.h
 * @author ZJX Marco
 * @date 2026/5/10
 */

#pragma once
#include "../types.h"

struct Statistics;
class StatisticsManager;

extern StatisticsManager stat;

struct Statistics {
    u64 cycles = 0;
    u64 retired = 0;

    u64 data_hazard_stalls = 0;
    u64 control_hazard_stalls = 0;
    u64 structural_hazard_stalls = 0;

    u64 branch_flushes = 0;
    u64 load_use_hazards = 0;

    u64 branches = 0;
    u64 branch_miss = 0;

    u64 loads  = 0;
    u64 stores = 0;

    u64 if_busy_cycles  = 0;
    u64 id_busy_cycles  = 0;
    u64 ex_busy_cycles  = 0;
    u64 mem_busy_cycles = 0;
    u64 wb_busy_cycles  = 0;

    u64 alu_instructions = 0;
    u64 load_instructions = 0;
    u64 store_instructions = 0;
    u64 branch_instructions = 0;
    u64 jump_instructions = 0;

};

class StatisticsManager {
public:
    StatisticsManager();

private:
    Statistics statistics;

    double ipc = 0.0;
    double cpi = 0.0;

};

