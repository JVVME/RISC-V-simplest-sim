//
// Created by zhang on 2026/5/9.
//

#include "cpu.h"
#include "../DebugTools/printer.h"
#include "../Statistics/statistics.h"
#include <cassert>
#include <iostream>


void CPU::reset() {
    current_pc = 0;
    next_pc = 0;
    current_pipeline = Pipeline{};
    next_pipeline = Pipeline{};
    current_register = RegisterFile{};
    next_register = RegisterFile{};
    halted = false;
    trace_enabled = true;
    cycles = 0;

    next_pc = current_pc;
    next_pipeline = current_pipeline;
    next_register = current_register;
    stat.reset();
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


    stat.onCycle();
    if (!halted) {
        stat.onIfBusyCycles();
    }
    if (current_pipeline.if_id.valid) {
        stat.onIdBusyCycles();
    }
    if (current_pipeline.id_ex.valid) {
        stat.onExBusyCycles();
    }
    if (current_pipeline.ex_mem.valid) {
        stat.onMemBusyCycles();
    }
    if (current_pipeline.mem_wb.valid) {
        stat.onWbBusyCycles();
    }

    fetch_stage();
    decode_stage();
    execute_stage();
    memory_stage();
    writeback_stage();

    commit();

    cycles++;

    if (trace_enabled) {
        print_cycle_trace(cycles, current_pc, halted, current_pipeline, current_register);
    }
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
    print_statistics(stat);
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
    stat.reset();
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


    const bool load_use_hazard = should_stall_for_load_use(inst);

    if (load_use_hazard) {
        stat.onLoadUseHazard();
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

    u32 rs1_value = current_register.read(inst.rs1);
    u32 rs2_value = current_register.read(inst.rs2);

    // ★ 新增：WB → ID 前递
    // WB 阶段写的是 next_register，在本周期 decode 读 current_register 时不可见。
    // 必须在这里手动前递，否则 Cycle N 的 WB 结果到 Cycle N+1 才能被 decode 看到，
    // 但 EX 阶段的 MEM_WB 转发在那时已经指向下一条指令了。
    if (can_forward_from_mem_wb(current_pipeline.mem_wb)) {
        const MEM_WB& wb = current_pipeline.mem_wb;
        if (uses_rs1(inst.op) && inst.rs1 != 0 && wb.rd == inst.rs1) {
            rs1_value = wb.writeback_value;
        }
        if (uses_rs2(inst.op) && inst.rs2 != 0 && wb.rd == inst.rs2) {
            rs2_value = wb.writeback_value;
        }
    }

    next_pipeline.id_ex = ID_EX{
        .valid = true,
        .rs1_value = rs1_value,
        .rs2_value = rs2_value,
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

    auto forward = compute_forwarding(id_ex);
    u32 forwarded_rs1 = resolve_forwarded_operand(forward.rs1, id_ex.rs1_value);
    u32 forwarded_rs2 = resolve_forwarded_operand(forward.rs2, id_ex.rs2_value);

    u32 op_a = forwarded_rs1;
    u32 op_b = ctrl.alu_src ? id_ex.imm : forwarded_rs2;
    u32 alu_result = ALU(ctrl.alu_op, op_a, op_b);

    bool take_branch = false;
    if (ctrl.branch) {
        stat.onBranches();
        switch (inst.op) {
            case RISCV32I::BEQ:
                take_branch = (forwarded_rs1 == forwarded_rs2);
                break;
            case RISCV32I::BNE:
                take_branch = (forwarded_rs1 != forwarded_rs2);
                break;
            default:
                break;
        }

        /*
        *当前采用的是“固定预测 not-taken（不跳）”。
        所以在条件分支里：
            实际结果 take_branch == false：预测正确（not-taken）
            实际结果 take_branch == true：预测错误（预测不跳，但实际跳了），这就是 branch miss
        更通用的写法其实是：if (predicted_taken != take_branch) miss++
        你现在 predicted_taken 恒为 false，所以就退化成了 if (take_branch) miss++
         */
        if (take_branch) {
            stat.onBranchMiss();
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
            target_pc = (forwarded_rs1 + id_ex.imm) & ~1u;
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
    out.rs2_value = forwarded_rs2; // SW 在 MEM 阶段会用到
    out.rd = inst.rd;
    out.inst = inst;
    out.ctrl = ctrl;
    next_pipeline.ex_mem = out;

    // 分支/跳转成立：改 PC，并冲刷流水线前两级
    if (take_branch || take_jump) {
        if (next_pipeline.if_id.valid) {
            stat.onControlHazardStall();
        }
        if (next_pipeline.id_ex.valid) {
            stat.onControlHazardStall();
        }
        stat.onBranchFlush(2);
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
        stat.onLoads();
        const u32 addr = ex_mem.alu_result;
        const u32 b0 = static_cast<u32>(memory->load_byte(addr));
        const u32 b1 = static_cast<u32>(memory->load_byte(addr + 1));
        const u32 b2 = static_cast<u32>(memory->load_byte(addr + 2));
        const u32 b3 = static_cast<u32>(memory->load_byte(addr + 3));
        writeback_value = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    // SW: 按 little-endian 拆成 4 字节写入
    if (ctrl.mem_write) {
        stat.onStores();
        const u32 addr = ex_mem.alu_result;
        const u32 data = ex_mem.rs2_value;
        memory->store_byte(addr,     static_cast<u8>( data        & 0xFF));
        memory->store_byte(addr + 1, static_cast<u8>((data >> 8)  & 0xFF));
        memory->store_byte(addr + 2, static_cast<u8>((data >> 16) & 0xFF));
        memory->store_byte(addr + 3, static_cast<u8>((data >> 24) & 0xFF));
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
    const Instruction& inst = mem_wb.inst;

    stat.onInstructionRetired();
    switch (inst.op) {
        case RISCV32I::ADD:
        case RISCV32I::SUB:
        case RISCV32I::AND:
        case RISCV32I::OR:
        case RISCV32I::XOR:
        case RISCV32I::ADDI:
        case RISCV32I::ANDI:
        case RISCV32I::ORI:
            stat.onAluInstructions();
            break;
        case RISCV32I::LW:
            stat.onLoadInstructions();
            break;
        case RISCV32I::SW:
            stat.onStoreInstructions();
            break;
        case RISCV32I::BEQ:
        case RISCV32I::BNE:
            stat.onBranchInstructions();
            break;
        case RISCV32I::JAL:
        case RISCV32I::JALR:
            stat.onJumpInstructions();
            break;
        case RISCV32I::NOP:
            break;
    }

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

bool CPU::uses_rs1(RISCV32I op) const {
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
}

bool CPU::uses_rs2(RISCV32I op) const {
    switch (op) {
        case RISCV32I::ADD: case RISCV32I::SUB: case RISCV32I::AND:
        case RISCV32I::OR:  case RISCV32I::XOR:
        case RISCV32I::SW:
        case RISCV32I::BEQ: case RISCV32I::BNE:
            return true;
        default:
            return false;
    }
}

bool CPU::writes_register(bool valid, const ControlSignals &ctrl, u8 rd) const {
    return valid && ctrl.reg_write && rd != 0;
}

bool CPU::can_forward_from_ex_mem(const EX_MEM &ex_mem) const {
    if (!ex_mem.valid) {
        return false;
    }

    if (!ex_mem.ctrl.reg_write) {
        return false;
    }

    if (ex_mem.rd == 0) {
        return false;
    }

    if (ex_mem.ctrl.mem_read) {
        return false;
    }

    return true;

}

bool CPU::can_forward_from_mem_wb(const MEM_WB &mem_wb) const {
    if (!mem_wb.valid) {
        return false;
    }

    if (!mem_wb.ctrl.reg_write) {
        return false;
    }

    if (mem_wb.rd == 0) {
        return false;
    }

    return true;
}

bool CPU::should_stall_for_load_use(const Instruction &inst) const {

    return current_pipeline.id_ex.valid &&
        current_pipeline.id_ex.ctrl.mem_read &&
        current_pipeline.id_ex.inst.rd != 0 &&
        ((uses_rs1(inst.op) && current_pipeline.id_ex.inst.rd == inst.rs1) ||
         (uses_rs2(inst.op) && current_pipeline.id_ex.inst.rd == inst.rs2));
}

CPU::ForwardSource CPU::choose_forward_source(u8 src_reg) const {
    if (src_reg == 0) {
        return ForwardSource::NONE;
    }

    if (can_forward_from_ex_mem(current_pipeline.ex_mem) && current_pipeline.ex_mem.rd == src_reg) {
        return ForwardSource::EX_MEM;
    }

    if (can_forward_from_mem_wb(current_pipeline.mem_wb) &&
        current_pipeline.mem_wb.rd == src_reg) {

        return ForwardSource::MEM_WB;
    }

    return ForwardSource::NONE;
}

CPU::ForwardDecision CPU::compute_forwarding(const ID_EX &id_ex) const {
    ForwardDecision decision;

    decision.rs1 = choose_forward_source(id_ex.inst.rs1);
    decision.rs2 = choose_forward_source(id_ex.inst.rs2);

    return decision;
}

u32 CPU::resolve_forwarded_operand(ForwardSource source, u32 original_value) const {

    switch (source) {
        case ForwardSource::EX_MEM:
            return current_pipeline.ex_mem.alu_result;
        case ForwardSource::MEM_WB:
            return current_pipeline.mem_wb.writeback_value;
        case ForwardSource::NONE:
        default:
            return original_value;
    }
}
