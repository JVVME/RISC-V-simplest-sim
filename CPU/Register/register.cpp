//
// Created by zhang on 2026/5/7.
//

#include "register.h"

#include <cassert>
#include <vector>

#include "../../types.h"

RegisterFile::RegisterFile() = default;

u32 RegisterFile::read(u8 rs) const {
    assert(rs < 32);
    return registers[rs];
}

void RegisterFile::write(u8 rs, u32 value) {
    assert(rs < 32);
    registers[rs] = value;
}


