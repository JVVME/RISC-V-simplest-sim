# RISC-V Simulator 一阶段

---

## 关于指令

### 最小 Pipeline 可运行子集

1. 算术/逻辑

   | 指令  | 作用 |
   |-----|----|
   | ADD | 加法 |
   | SUB | 减法 |
   | AND | 与  |
   | OR  | 或  |
   | XOR | 异或 |

2. 立即数

   I-type

   | 指令   | 作用   |
   |------|------|
   | ADDI | 加立即数 |
   | ANDI | 与立即数 |
   | ORI  | 或立即数 |

3. Load/Store

    | 指令 | 作用         |
    |----|------------|
    | LW | load word  |
    | SW | store word |

4. 分支

    | 指令  | 作用   |
    |-----|------|
    | BEQ | 相等跳转 |
    | BNE | 不等跳转 |

5. 跳转

    | 指令   | 作用            |
    |------|---------------|
    | JAL  | jump and link |
    | JALR | indirect jump |


---

## 整体结构说明


---

## 初始化说明



---



