//
// Created by zhang on 2026/5/8.
//

#include "printer.h"

#include <iomanip>
#include <iostream>
#include <string>

namespace {

std::string opcode_to_string(const RISCV32I op) {

    switch (op) {

    case RISCV32I::NOP:
        return "NOP";

    case RISCV32I::ADD:
        return "ADD";

    case RISCV32I::SUB:
        return "SUB";

    case RISCV32I::AND:
        return "AND";

    case RISCV32I::OR:
        return "OR";

    case RISCV32I::XOR:
        return "XOR";

    case RISCV32I::ADDI:
        return "ADDI";

    case RISCV32I::ANDI:
        return "ANDI";

    case RISCV32I::ORI:
        return "ORI";

    case RISCV32I::LW:
        return "LW";

    case RISCV32I::SW:
        return "SW";

    case RISCV32I::BEQ:
        return "BEQ";

    case RISCV32I::BNE:
        return "BNE";

    case RISCV32I::JAL:
        return "JAL";

    case RISCV32I::JALR:
        return "JALR";
    }

    return "UNKNOWN";
}

} // namespace

void print_program(const std::vector<Instruction>& program) {

    for (const auto& inst : program) {

        std::cout
            << std::right
            << std::setw(8)
            << std::setfill('0')
            << std::hex
            << inst.pc
            << ": ";

        const auto op = opcode_to_string(inst.op);

        std::cout
            << std::left
            << std::setw(6)
            << std::setfill(' ')
            << op;

        std::cout << std::dec;

        switch (inst.op) {

        case RISCV32I::NOP:
            break;

        case RISCV32I::ADD:
        case RISCV32I::SUB:
        case RISCV32I::AND:
        case RISCV32I::OR:
        case RISCV32I::XOR:

            std::cout
                << "x" << static_cast<int>(inst.rd)
                << ", x" << static_cast<int>(inst.rs1)
                << ", x" << static_cast<int>(inst.rs2);

            break;

        case RISCV32I::ADDI:
        case RISCV32I::ANDI:
        case RISCV32I::ORI:

            std::cout
                << "x" << static_cast<int>(inst.rd)
                << ", x" << static_cast<int>(inst.rs1)
                << ", " << std::dec << inst.imm;

            break;

        case RISCV32I::LW:

            std::cout
                << "x" << static_cast<int>(inst.rd)
                << ", "
                << std::dec
                << inst.imm
                << "(x"
                << static_cast<int>(inst.rs1)
                << ")";

            break;

        case RISCV32I::SW:

            std::cout
                << "x" << static_cast<int>(inst.rs2)
                << ", "
                << std::dec
                << inst.imm
                << "(x"
                << static_cast<int>(inst.rs1)
                << ")";

            break;

        case RISCV32I::BEQ:
        case RISCV32I::BNE:

            std::cout
                << "x" << static_cast<int>(inst.rs1)
                << ", x" << static_cast<int>(inst.rs2)
                << ", "
                << std::dec
                << inst.imm;

            break;

        case RISCV32I::JAL:

            std::cout
                << "x" << static_cast<int>(inst.rd)
                << ", "
                << std::dec
                << inst.imm;

            break;

        case RISCV32I::JALR:

            std::cout
                << "x" << static_cast<int>(inst.rd)
                << ", "
                << std::dec
                << inst.imm
                << "(x"
                << static_cast<int>(inst.rs1)
                << ")";

            break;
        }

        std::cout << '\n';
    }
}

void print_cycle_trace(
    u64 cycle,
    u32 pc,
    bool halted,
    const Pipeline& pipeline,
    const RegisterFile& regfile
) {
    std::cout << std::right;
    std::cout << "\n========== CYCLE " << cycle << " ==========\n";
    std::cout << "PC=0x" << std::hex << std::setw(8) << std::setfill('0') << pc
              << std::dec << std::setfill(' ')
              << " halted=" << halted << "\n";

    const auto& if_id = pipeline.if_id;
    const auto& id_ex = pipeline.id_ex;
    const auto& ex_mem = pipeline.ex_mem;
    const auto& mem_wb = pipeline.mem_wb;

    std::cout << "[IF/ID] valid=" << if_id.valid;
    if (if_id.valid) {
        std::cout << " op=" << opcode_to_string(if_id.inst.op);
    }
    std::cout << "\n";

    std::cout << "[ID/EX] valid=" << id_ex.valid;
    if (id_ex.valid) {
        std::cout << " op=" << opcode_to_string(id_ex.inst.op)
                  << " rs1v=0x" << std::hex << id_ex.rs1_value
                  << " rs2v=0x" << id_ex.rs2_value
                  << " imm=0x" << id_ex.imm << std::dec;
    }
    std::cout << "\n";

    std::cout << "[EX/MEM] valid=" << ex_mem.valid;
    if (ex_mem.valid) {
        std::cout << " op=" << opcode_to_string(ex_mem.inst.op)
                  << " alu=0x" << std::hex << ex_mem.alu_result << std::dec
                  << " rd=x" << static_cast<int>(ex_mem.rd);
    }
    std::cout << "\n";

    std::cout << "[MEM/WB] valid=" << mem_wb.valid;
    if (mem_wb.valid) {
        std::cout << " op=" << opcode_to_string(mem_wb.inst.op)
                  << " wb=0x" << std::hex << mem_wb.writeback_value << std::dec
                  << " rd=x" << static_cast<int>(mem_wb.rd);
    }
    std::cout << "\n";

    std::cout << "REGFILE\n";
    for (int i = 0; i < 32; ++i) {
        const u32 v = regfile.read(static_cast<u8>(i));
        std::cout << "x" << std::setw(2) << std::setfill('0') << i
                  << "=0x" << std::hex << std::setw(8) << v << std::dec
                  << std::setfill(' ') << ((i % 4 == 3) ? '\n' : ' ');
    }
}

void print_final_registers(u64 total_cycles, const RegisterFile& regfile) {
    std::cout << std::right;
    std::cout << "\n===== PROGRAM FINISHED =====\n";
    std::cout << "total cycles = " << total_cycles << "\n";
    for (int i = 0; i < 32; ++i) {
        const u32 v = regfile.read(static_cast<u8>(i));
        std::cout << "x" << std::setw(2) << std::setfill('0') << i
                  << "=0x" << std::hex << std::setw(8) << v << std::dec
                  << std::setfill(' ') << ((i % 4 == 3) ? '\n' : ' ');
    }
    std::cout << std::right;
}

void print_statistics(const StatisticsManager& stat_manager) {
    const Statistics& s = stat_manager.snapshot();
    auto safe_ratio = [](u64 numerator, u64 denominator) -> double {
        return denominator == 0
                   ? 0.0
                   : static_cast<double>(numerator) / static_cast<double>(denominator);
    };

    std::cout << "\n===== STATISTICS =====\n";
    std::cout << "cycles               = " << s.cycles << "\n";
    std::cout << "retired              = " << s.retired << "\n";
    std::cout << "IPC                  = " << std::fixed << std::setprecision(4) << stat_manager.getIPC() << "\n";
    std::cout << "CPI                  = " << std::fixed << std::setprecision(4) << stat_manager.getCPI() << "\n";

    std::cout << "data_hazard_stalls   = " << s.data_hazard_stalls << "\n";
    std::cout << "control_hazard_stalls= " << s.control_hazard_stalls << "\n";
    std::cout << "structural_hazard_stalls = " << s.structural_hazard_stalls << "\n";
    std::cout << "load_use_hazards     = " << s.load_use_hazards << "\n";
    std::cout << "branch_flushes       = " << s.branch_flushes << "\n";

    std::cout << "branches             = " << s.branches << "\n";
    std::cout << "branch_miss          = " << s.branch_miss << "\n";
    std::cout << "branch_miss_rate     = " << std::fixed << std::setprecision(4)
              << safe_ratio(s.branch_miss, s.branches) << "\n";

    std::cout << "loads                = " << s.loads << "\n";
    std::cout << "stores               = " << s.stores << "\n";

    std::cout << "if_busy_cycles       = " << s.if_busy_cycles << "\n";
    std::cout << "id_busy_cycles       = " << s.id_busy_cycles << "\n";
    std::cout << "ex_busy_cycles       = " << s.ex_busy_cycles << "\n";
    std::cout << "mem_busy_cycles      = " << s.mem_busy_cycles << "\n";
    std::cout << "wb_busy_cycles       = " << s.wb_busy_cycles << "\n";

    std::cout << "alu_instructions     = " << s.alu_instructions << "\n";
    std::cout << "load_instructions    = " << s.load_instructions << "\n";
    std::cout << "store_instructions   = " << s.store_instructions << "\n";
    std::cout << "branch_instructions  = " << s.branch_instructions << "\n";
    std::cout << "jump_instructions    = " << s.jump_instructions << "\n";
}


