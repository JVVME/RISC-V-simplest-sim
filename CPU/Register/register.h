/**
 * @file register.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once
#include "../../types.h"

class RegisterFile;

class RegisterFile {
public:
    RegisterFile();
    u32 read(u8 rs) const;
    void write(u8 rs, u32 value);
private:
    word_t registers[32]{};
};
