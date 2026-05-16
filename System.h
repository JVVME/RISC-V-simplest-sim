/**
 * @file System.h
 * @author ZJX Marco
 * @date 2026/5/15
 */

#pragma once
#include "CPU/cpu.h"
#include "CPU/Cache/cache.h"
#include "Memory/memory.h"

class System {
public:
    System();

    void tick();

private:
    Memory memory;

    Cache L1_Cache;

    CPU cpu;
};
