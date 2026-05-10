//
// Created by zhang on 2026/5/7.
//

#include "memory.h"

#include <cassert>

Memory::Memory() = default;

u8 Memory::load_word(u32 addr) const {
    if (addr >= MemorySize) {
        assert(false);
    }
    return memory[addr];
}

void Memory::store_word(u32 addr, u8 value) {
    if (addr >= MemorySize) {
        assert(false);
    }
    memory[addr] = value;
}
