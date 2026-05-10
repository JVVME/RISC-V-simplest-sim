/**
 * @file memory.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once
#include <vector>

#include "../types.h"

class Memory;

class Memory {
public:
    Memory();
    u8 load_word(u32 addr) const;
    void store_word(u32 addr, u8 value);
private:
    std::vector<u8> memory = std::vector<u8>(MemorySize, 0);
};