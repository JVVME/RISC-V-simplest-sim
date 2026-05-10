cmake_minimum_required(VERSION 4.2)
project(RISCV_Simulator)

set(CMAKE_CXX_STANDARD 26)

add_executable(RISCV_Simulator main.cpp
        types.h
        ISA/instruction.cpp
        ISA/instruction.h
        CPU/ALU/alu.cpp
        CPU/ALU/alu.h
        CPU/Register/register.cpp
        CPU/Register/register.h
        Memory/memory.cpp
        Memory/memory.h
        DebugTools/printer.cpp
        DebugTools/printer.h
        CPU/Pipeline/pipeline.cpp
        CPU/Pipeline/pipeline.h
        CPU/cpu.cpp
        CPU/cpu.h
        Statistics/statistics.cpp
        Statistics/statistics.h)
