#include <iostream>
#include "./ISA/instruction.h"
#include "./DebugTools/printer.h"
#include "./CPU/cpu.h"

int main() {
    std::string path = R"(C:\Users\zhang\Marco\RISCV_Simulator\Generated\Instructions\Assembly\full_demo1.txt)";
    std::vector<Instruction> instructions;
    instructions = parse_assembly_file(path);
    print_program(instructions);

    Memory memory;
    Cache cache(4096, 16, CacheReplacePolicy::LRU, CacheWritePolicy::WriteThrough, &memory);
    CPU cpu(&cache, &memory);

    cpu.setProgram(instructions, 0);
    cpu.run();

    return 0;
}
