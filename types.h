/**
 * @file types.h
 * @author ZJX Marco
 * @date 2026/5/7
 */

#pragma once
#include <cstdint>

constexpr uint32_t MemorySize = 4096;

constexpr uint32_t cacheline_size = 64;

constexpr uint32_t block_cache_miss_latency = 30;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i32 = int32_t;

using word_t = u32;
