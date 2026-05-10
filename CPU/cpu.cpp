//
// Created by zhang on 2026/5/9.
//

#include "cpu.h"
#include "../DebugTools/printer.h"
#include <cassert>

CPU::CPU() {
    reset();
}

void CPU::reset() {
    current_pc = 0;
    next_pc = 0;
    current_pipeline = Pipeline{};
    next_pipeline = Pipeline{};
    current_register = RegisterFile{};
    next_register = RegisterFile{};
    halted = false;
    cycles = 0;

    next_pc = current_pc;
    next_pipeline = current_pipeline;
    next_register = current_register;
}

void CPU::tick() {

    // 已停机且流水线为空：不再推进
    const bool pipeline_busy =
        current_pipeline.if_id.valid ||
        current_pipeline.id_ex.valid ||
        current_pipeline.ex_mem.valid ||
        current_pipeline.mem_wb.valid;

    if (halted && !pipeline_busy) {
        return;
    }

    current_pc = next_pc;
    current_pipeline = next_pipeline;
    current_register = next_register;

    fetch_stage();
    decode_stage();
    execute_stage();
    memory_stage();
    writeback_stage();

    commit();

    cycles++;

    print_cycle_trace(cycles, current_pc, halted, current_pipeline, current_register);
}

void CPU::run() {
    auto pipeline_busy = [this]() {
        return current_pipeline.if_id.valid ||
               current_pipeline.id_ex.valid ||
               current_pipeline.ex_mem.valid ||
               current_pipeline.mem_wb.valid;
    };

    // halted 后继续跑，直到流水线排空
    while (!halted || pipeline_busy()) {
        tick();
    }
    print_final_registers(cycles, current_register);
}

void CPU::setProgram(std::vector<Instruction>& program, u32 init_pc) {
    this->program = program;
    this->current_pc = init_pc;
    this->next_pc = init_pc;

    // 避免旧流水线残留影响新程序
    this->current_pipeline = Pipeline{};
    this->next_pipeline = Pipeline{};

    this->halted = false;
    this->cycles = 0;
}

void CPU::fetch_stage() {
    if (halted) {
        next_pipeline.if_id = IF_ID{};
        next_pc = current_pc;
        return;
    }

    assert((current_pc & 0x3u) == 0); // PC 4 字节对齐

    const u32 instruction_index = current_pc >> 2;
    if (instruction_index >= program.size()) {
        // 程序取指结束：停止发射新指令
        halted = true;
        next_pipeline.if_id = IF_ID{};
        next_pc = current_pc;
        return;
    }

    Instruction instruction = program[instruction_index];
    next_pipeline.if_id = IF_ID{
        .valid = true,
        .inst = instruction,
    };
    next_pc = current_pc + 4;
}

void CPU::decode_stage() {
    if (!current_pipeline.if_id.valid) {
        next_pipeline.id_ex = ID_EX{};
        return ;
    }
    const Instruction& inst = current_pipeline.if_id.inst;

    auto uses_rs1 = [](RISCV32I op) -> bool {
        switch (op) {
            case RISCV32I::ADD: case RISCV32I::SUB: case RISCV32I::AND:
            case RISCV32I::OR:  case RISCV32I::XOR:
            case RISCV32I::ADDI: case RISCV32I::ANDI: case RISCV32I::ORI:
            case RISCV32I::LW: case RISCV32I::SW:
            case RISCV32I::BEQ: case RISCV32I::BNE:
            case RISCV32I::JALR:
                return true;
            default:
                return false;
        }
    };

    auto uses_rs2 = [](RISCV32I op) -> bool {
        switch (op) {
            case RISCV32I::ADD: case RISCV32I::SUB: case RISCV32I::AND:
            case RISCV32I::OR:  case RISCV32I::XOR:
            case RISCV32I::SW:
            case RISCV32I::BEQ: case RISCV32I::BNE:
                return true;
            default:
                return false;
        }
    };

    auto stage_writes = [](bool valid, const ControlSignals& ctrl, u8 rd) -> bool {
        return valid && ctrl.reg_write && rd != 0;
    };

    auto has_conflict = [&](u8 src) -> bool {
        if (src == 0) return false;

        if (stage_writes(current_pipeline.id_ex.valid,
                         current_pipeline.id_ex.ctrl,
                         current_pipeline.id_ex.inst.rd) &&
            current_pipeline.id_ex.inst.rd == src) {
            return true;
            }

        if (stage_writes(current_pipeline.ex_mem.valid,
                         current_pipeline.ex_mem.ctrl,
                         current_pipeline.ex_mem.rd) &&
            current_pipeline.ex_mem.rd == src) {
            return true;
            }

        if (stage_writes(current_pipeline.mem_wb.valid,
                         current_pipeline.mem_wb.ctrl,
                         current_pipeline.mem_wb.rd) &&
            current_pipeline.mem_wb.rd == src) {
            return true;
            }

        return false;
    };

    const bool stall = (uses_rs1(inst.op) && has_conflict(inst.rs1)) || (uses_rs2(inst.op) && has_conflict(inst.rs2));

    if (stall) {
        // 插入 bubble，冻结 IF/PC
        next_pipeline.id_ex = ID_EX{};
        next_pipeline.if_id = current_pipeline.if_id;
        next_pc = current_pc;
        return;
    }

    ControlSignals control_signals{};
    /*
    R 型（ADD/SUB/AND/OR/XOR）
    reg_write=true，因为结果要写回 rd。
    alu_src=false，第二操作数来自 rs2。
    alu_op 分别设为对应运算，供 EX 阶段直接执行。
    mem_read/mem_write/mem_to_reg 都是 false，因为不访存。

    I 型算术（ADDI/ANDI/ORI）
    reg_write=true，结果写 rd。
    alu_src=true，第二操作数选立即数 imm。
    alu_op 设为 ADD/AND/OR，对应指令语义。
    不访存。

    LW
    reg_write=true，最终要写回寄存器。
    mem_read=true，需要从内存读。
    mem_to_reg=true，写回数据来自内存而不是 ALU。
    alu_src=true + alu_op=ADD，用 base + imm 计算地址。

    SW
    mem_write=true，把 rs2 写到内存。
    reg_write=false，因为不写寄存器。
    alu_src=true + alu_op=ADD，同样先算 base + imm 地址。

    BEQ/BNE
    branch=true，交给 EX 做分支判断和改 PC。
    alu_src=false，比较的是 rs1 与 rs2。
    alu_op=SUB，常见做法是看减法结果是否为 0 来判断相等。
    不写回、不访存。

    JAL/JALR
    jump=true，表示无条件跳转类控制流。
    reg_write=true，因为要写返回地址（rd <- pc+4）。
    alu_src=true + alu_op=ADD，用于目标地址计算（pc+imm 或 rs1+imm，具体在 EX 实现）。

    NOP
    全部关闭（全 false + ALUOp::NOP），等价于气泡。
     */
    switch (inst.op) {
        case RISCV32I::NOP:
            control_signals = {
                .reg_write = false, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::NOP
            };
            break;
        case RISCV32I::ADD:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::ADD
            };
            break;
        case RISCV32I::SUB:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::SUB
            };
            break;
        case RISCV32I::AND:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::AND
            };
            break;
        case RISCV32I::OR:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::OR
            };
            break;
        case RISCV32I::XOR:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = false, .alu_op = ALUOp::XOR
            };
            break;
        case RISCV32I::ADDI:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = true, .alu_op = ALUOp::ADD
            };
            break;
        case RISCV32I::ANDI:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = true, .alu_op = ALUOp::AND
            };
            break;
        case RISCV32I::ORI:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = true, .alu_op = ALUOp::OR
            };
            break;
        case RISCV32I::LW:
            control_signals = {
                .reg_write = true, .mem_read = true, .mem_write = false, .mem_to_reg = true,
                .branch = false, .jump = false, .alu_src = true, .alu_op = ALUOp::ADD
            };
            break;
        case RISCV32I::SW:
            control_signals = {
                .reg_write = false, .mem_read = false, .mem_write = true, .mem_to_reg = false,
                .branch = false, .jump = false, .alu_src = true, .alu_op = ALUOp::ADD
            };
            break;
        case RISCV32I::BEQ:
            control_signals = {
                .reg_write = false, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = true, .jump = false, .alu_src = false, .alu_op = ALUOp::SUB
            };
            break;
        case RISCV32I::BNE:
            control_signals = {
                .reg_write = false, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = true, .jump = false, .alu_src = false, .alu_op = ALUOp::SUB
            };
            break;
        case RISCV32I::JAL:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = true, .alu_src = true, .alu_op = ALUOp::ADD
            };
            break;
        case RISCV32I::JALR:
            control_signals = {
                .reg_write = true, .mem_read = false, .mem_write = false, .mem_to_reg = false,
                .branch = false, .jump = true, .alu_src = true, .alu_op = ALUOp::ADD
            };
            break;
        default:
            assert(false);
            break;
    }

    next_pipeline.id_ex = ID_EX{
        .valid = true,
        .rs1_value = current_register.read(inst.rs1),
        .rs2_value = current_register.read(inst.rs2),
        .imm = static_cast<u32>(inst.imm),
        .inst = inst,
        .ctrl = control_signals,
    };
}

void CPU::execute_stage() {
    if (!current_pipeline.id_ex.valid) {
        next_pipeline.ex_mem = EX_MEM{};
        return ;
    }

    const ID_EX& id_ex = current_pipeline.id_ex;
    const Instruction& inst = id_ex.inst;
    const ControlSignals& ctrl = id_ex.ctrl;

    u32 op_a = id_ex.rs1_value;
    u32 op_b = ctrl.alu_src ? id_ex.imm : id_ex.rs2_value;
    u32 alu_result = ALU(ctrl.alu_op, op_a, op_b);

    bool take_branch = false;
    if (ctrl.branch) {
        switch (inst.op) {
            case RISCV32I::BEQ:
                take_branch = (id_ex.rs1_value == id_ex.rs2_value);
                break;
            case RISCV32I::BNE:
                take_branch = (id_ex.rs1_value != id_ex.rs2_value);
                break;
            default:
                break;
        }
    }

    bool take_jump = ctrl.jump;
    u32 target_pc = current_pc + 4;

    if (take_branch) {
        target_pc = inst.pc + id_ex.imm;
    }

    if (take_jump) {
        if (inst.op == RISCV32I::JAL) {
            target_pc = inst.pc + id_ex.imm;
        }
        else if (inst.op == RISCV32I::JALR) {
            target_pc = (id_ex.rs1_value + id_ex.imm) & ~1u;
        }
    }

    // JAL/JALR 写回 rd 的值是 pc+4，不是跳转目标
    u32 ex_result = alu_result;
    if (inst.op == RISCV32I::JAL || inst.op == RISCV32I::JALR) {
        ex_result = inst.pc +4;
    }

    EX_MEM out{};
    out.valid = true;
    out.alu_result = ex_result;
    out.rs2_value = id_ex.rs2_value; // SW 在 MEM 阶段会用到
    out.rd = inst.rd;
    out.inst = inst;
    out.ctrl = ctrl;
    next_pipeline.ex_mem = out;

    // 分支/跳转成立：改 PC，并冲刷流水线前两级
    if (take_branch || take_jump) {
        halted = false;
        next_pc = target_pc;
        next_pipeline.if_id = IF_ID{};
        next_pipeline.id_ex = ID_EX{};
    }

}

void CPU::memory_stage() {
    if (!current_pipeline.ex_mem.valid) {
        next_pipeline.mem_wb = MEM_WB{};
        return;
    }

    const EX_MEM& ex_mem = current_pipeline.ex_mem;
    const ControlSignals& ctrl = ex_mem.ctrl;

    u32 writeback_value = ex_mem.alu_result;

    // LW: 按 little-endian 读取 4 字节拼成 32 位
    if (ctrl.mem_read) {
        const u32 addr = ex_mem.alu_result;
        const u32 b0 = static_cast<u32>(memory.load_word(addr));
        const u32 b1 = static_cast<u32>(memory.load_word(addr + 1));
        const u32 b2 = static_cast<u32>(memory.load_word(addr + 2));
        const u32 b3 = static_cast<u32>(memory.load_word(addr + 3));
        writeback_value = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    // SW: 按 little-endian 拆成 4 字节写入
    if (ctrl.mem_write) {
        const u32 addr = ex_mem.alu_result;
        const u32 data = ex_mem.rs2_value;
        memory.store_word(addr,     static_cast<u8>( data        & 0xFF));
        memory.store_word(addr + 1, static_cast<u8>((data >> 8)  & 0xFF));
        memory.store_word(addr + 2, static_cast<u8>((data >> 16) & 0xFF));
        memory.store_word(addr + 3, static_cast<u8>((data >> 24) & 0xFF));
    }

    MEM_WB out{};
    out.valid = true;
    out.writeback_value = writeback_value;
    out.rd = ex_mem.rd;
    out.inst = ex_mem.inst;
    out.ctrl = ctrl;
    next_pipeline.mem_wb = out;

}

void CPU::writeback_stage() {
    if (!current_pipeline.mem_wb.valid) {
        // 保证 x0 恒为 0
        next_register.write(0, 0);
        return;
    }

    const MEM_WB& mem_wb = current_pipeline.mem_wb;
    const ControlSignals& ctrl = mem_wb.ctrl;

    // 只有需要写回的指令才写寄存器，并且禁止写 x0
    if (ctrl.reg_write && mem_wb.rd != 0) {
        next_register.write(mem_wb.rd, mem_wb.writeback_value);
    }

    // 再次兜底 x0 恒为 0
    next_register.write(0, 0);

}

void CPU::commit() {
    current_pc = next_pc;
    current_pipeline = next_pipeline;
    current_register = next_register;
}
