//
// Created by zhang on 2026/5/7.
//

#include "alu.h"

/**
 * @brief 算术逻辑单元 (ALU)
 *
 * @param op 执行的操作类型 (枚举)
 * @param a  第一个操作数 (通常是 Rs1)
 * @param b  第二个操作数 (通常是 Rs2 或 立即数)
 * @return word_t 运算结果
 */
word_t ALU(const ALUOp op, const word_t a, const word_t b) {
    switch (op) {
        // --- 基础算术运算 ---
        case ALUOp::ADD:  return a + b;           // 加法
        case ALUOp::SUB:  return a - b;           // 减法

            // --- 逻辑运算 ---
        case ALUOp::AND:  return a & b;           // 按位与
        case ALUOp::OR:   return a | b;           // 按位或
        case ALUOp::XOR:  return a ^ b;           // 按位异或

            // --- 移位运算 ---
            // 注意：b & 0x1F 是为了限制位移量在 0-31 之间（针对 32 位字长）
        case ALUOp::SLL:  return a << (b & 0x1F); // 逻辑左移 (Shift Left Logical)
        case ALUOp::SRL:  return a >> (b & 0x1F); // 逻辑右移 (Shift Right Logical)，高位补 0

            // 算术右移 (Shift Right Arithmetic)：强制转换为有符号数，高位补符号位
        case ALUOp::SRA:  return static_cast<int32_t>(a) >> (b & 0x1F);

            // --- 比较运算 ---
        case ALUOp::SLT:
            // 有符号小于则置位 (Set Less Than)：将操作数视为有符号整数进行比较
            return (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1 : 0;

        case ALUOp::SLTU:
            // 无符号小于则置位 (Set Less Than Unsigned)：直接按无符号数比较
            return (a < b) ? 1 : 0;

            // --- 默认情况 ---
        default:
            return 0; // 未定义操作返回 0
    }
}
