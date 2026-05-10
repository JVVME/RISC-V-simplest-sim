//
// Created by zhang on 2026/5/10.
//

#include "statistics.h"

StatisticsManager::StatisticsManager() = default;

StatisticsManager stat;

void StatisticsManager::reset() {
    this->statistics = {};
    this->ipc = 0.0;
    this->cpi = 0.0;
}

void StatisticsManager::onCycle() {
    this->statistics.cycles++;
    updateDerived();
}

void StatisticsManager::onInstructionRetired() {
    this->statistics.retired++;
    updateDerived();
}

void StatisticsManager::onDataHazardStall() {
    this->statistics.data_hazard_stalls++;
}

void StatisticsManager::onControlHazardStall() {
    this->statistics.control_hazard_stalls++;
}

void StatisticsManager::onStructuralHazardStall() {
    this->statistics.structural_hazard_stalls++;
}

void StatisticsManager::onBranchFlush() {
    this->statistics.branch_flushes++;
}

void StatisticsManager::onLoadUseHazard() {
    this->statistics.load_use_hazards++;
}

void StatisticsManager::onBranches() {
    this->statistics.branches++;
}

void StatisticsManager::onBranchMiss() {
    this->statistics.branch_miss++;
}

void StatisticsManager::onLoads() {
    this->statistics.loads++;
}

void StatisticsManager::onStores() {
    this->statistics.stores++;
}

void StatisticsManager::onIfBusyCycles() {
    this->statistics.if_busy_cycles++;
}

void StatisticsManager::onIdBusyCycles() {
    this->statistics.id_busy_cycles++;
}

void StatisticsManager::onExBusyCycles() {
    this->statistics.ex_busy_cycles++;
}

void StatisticsManager::onMemBusyCycles() {
    this->statistics.mem_busy_cycles++;
}

void StatisticsManager::onWbBusyCycles() {
    this->statistics.wb_busy_cycles++;
}

void StatisticsManager::onAluInstructions() {
    this->statistics.alu_instructions++;
}

void StatisticsManager::onLoadInstructions() {
    this->statistics.load_instructions++;
}

void StatisticsManager::onStoreInstructions() {
    this->statistics.store_instructions++;
}

void StatisticsManager::onBranchInstructions() {
    this->statistics.branch_instructions++;
}

void StatisticsManager::onJumpInstructions() {
    this->statistics.jump_instructions++;
}

const Statistics& StatisticsManager::snapshot() const {
    return this->statistics;
}

double StatisticsManager::getIPC() const {
    return this->ipc;
}

double StatisticsManager::getCPI() const {
    return this->cpi;
}

void StatisticsManager::updateDerived() {
    const auto cycles = this->statistics.cycles;
    const auto retired = this->statistics.retired;

    this->ipc = (cycles == 0)
                    ? 0.0
                    : static_cast<double>(retired) / static_cast<double>(cycles);

    this->cpi = (retired == 0)
                    ? 0.0
                    : static_cast<double>(cycles) / static_cast<double>(retired);
}
