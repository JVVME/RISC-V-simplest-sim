//
// Created by zhang on 2026/5/7.
//

#include "memory.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>

Memory::Memory() = default;

u8 Memory::load_byte(u32 addr) {
    checkAddress(addr);
    const u8 value = memory[addr];
    recordAccess(false, addr, value);
    return value;
}

void Memory::store_byte(u32 addr, u8 value) {
    checkAddress(addr);
    memory[addr] = value;
    recordAccess(true, addr, value);
}

void Memory::setDebugContext(u64 cycle, u32 pc) {
    current_cycle = cycle;
    current_pc = pc;
}

void Memory::setTrace(bool enable) {
    trace_enabled = enable;
}

void Memory::addWatchpoint(u32 addr) {
    checkAddress(addr);
    watchpoints.insert(addr);
}

bool Memory::hasWatchpoints() const {
    return !watchpoints.empty();
}

void Memory::clearWatchpoints() {
    watchpoints.clear();
}

void Memory::clearAccessLog() {
    access_log.clear();
}

void Memory::printAccessLog(std::ostream& os, std::size_t max_entries) const {
    const std::size_t total = access_log.size();
    const std::size_t begin = (max_entries == 0 || max_entries >= total) ? 0 : (total - max_entries);

    os << "\n===== MEMORY ACCESS LOG =====\n";
    os << "total entries = " << total << "\n";

    for (std::size_t i = begin; i < total; ++i) {
        const MemoryAccess& entry = access_log[i];
        os << "#" << entry.seq
           << " cycle=" << entry.cycle
           << " pc=0x" << std::hex << std::setw(8) << std::setfill('0') << entry.pc
           << std::dec << std::setfill(' ')
           << " " << (entry.is_write ? "W" : "R")
           << " addr=0x" << std::hex << std::setw(8) << std::setfill('0') << entry.addr
           << " value=0x" << std::setw(2) << static_cast<unsigned>(entry.value)
           << std::dec << std::setfill(' ') << "\n";
    }
}

void Memory::printWatchpointValues(std::ostream& os) const {
    if (watchpoints.empty()) {
        return;
    }

    std::vector<u32> addresses(watchpoints.begin(), watchpoints.end());
    std::ranges::sort(addresses);

    os << "\n===== WATCHPOINT VALUES =====\n";
    for (const u32 addr : addresses) {
        os << "addr=0x" << std::hex << std::setw(8) << std::setfill('0') << addr
           << " value=0x" << std::setw(2) << static_cast<unsigned>(memory[addr])
           << std::dec << std::setfill(' ') << "\n";
    }
}

void Memory::dumpBytes(u32 start, u32 len, std::ostream& os) const {
    if (len == 0) {
        os << "\n===== MEMORY DUMP =====\n(empty)\n";
        return;
    }

    const u32 end_exclusive = start + len;
    assert(end_exclusive >= start);
    assert(end_exclusive <= MemorySize);

    os << "\n===== MEMORY DUMP =====\n";
    for (u32 addr = start; addr < end_exclusive; addr += 16) {
        os << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr << ": ";

        for (u32 i = 0; i < 16; ++i) {
            const u32 cur = addr + i;
            if (cur < end_exclusive) {
                os << std::setw(2) << static_cast<unsigned>(memory[cur]) << " ";
            } else {
                os << "   ";
            }
        }
        os << std::dec << std::setfill(' ') << "\n";
    }
}

void Memory::checkAddress(u32 addr) const {
    if (addr >= MemorySize) {
        assert(false);
    }
}

void Memory::recordAccess(bool is_write, u32 addr, u8 value) {
    ++access_seq;

    if (trace_enabled) {
        access_log.push_back(MemoryAccess{
            .seq = access_seq,
            .cycle = current_cycle,
            .pc = current_pc,
            .is_write = is_write,
            .addr = addr,
            .value = value,
        });
    }

    if (watchpoints.contains(addr)) {
        std::cout << "[MEM-WATCH] cycle=" << current_cycle
                  << " pc=0x" << std::hex << std::setw(8) << std::setfill('0') << current_pc
                  << std::dec << std::setfill(' ')
                  << " #" << access_seq
                  << " " << (is_write ? "WRITE" : "READ")
                  << " addr=0x" << std::hex << std::setw(8) << std::setfill('0') << addr
                  << " value=0x" << std::setw(2) << static_cast<unsigned>(value)
                  << std::dec << std::setfill(' ') << "\n"
                  << std::flush;
    }
}
