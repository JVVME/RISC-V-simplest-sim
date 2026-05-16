/**
 * @file memory.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <unordered_set>
#include <vector>

#include "../types.h"

class MemoryInterface;
class Memory;

struct MemoryAccess {
    u64 seq = 0;
    u64 cycle = 0;
    u32 pc = 0;
    bool is_write = false;
    u32 addr = 0;
    u8 value = 0;
};

class MemoryInterface {
public:
    virtual ~MemoryInterface() = default;

    virtual u8 load_byte(u32 addr) = 0;
    virtual void store_byte(u32 addr, u8 value) = 0;
};

class Memory : public MemoryInterface {
public:
    Memory();
    u8 load_byte(u32 addr) override;
    void store_byte(u32 addr, u8 value) override;

    void setDebugContext(u64 cycle, u32 pc);
    void setTrace(bool enable);
    void addWatchpoint(u32 addr);
    bool hasWatchpoints() const;
    void clearWatchpoints();
    void clearAccessLog();
    void printAccessLog(std::ostream& os, std::size_t max_entries = 0) const;
    void printWatchpointValues(std::ostream& os) const;
    void dumpBytes(u32 start, u32 len, std::ostream& os) const;

private:
    void checkAddress(u32 addr) const;
    void recordAccess(bool is_write, u32 addr, u8 value);

    bool trace_enabled = false;
    u64 access_seq = 0;
    u64 current_cycle = 0;
    u32 current_pc = 0;
    std::vector<MemoryAccess> access_log;
    std::unordered_set<u32> watchpoints;
    std::vector<u8> memory = std::vector<u8>(MemorySize, 0);
};
