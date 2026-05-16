/**
 * @file cache.h
 * @author ZJX Marco
 * @date 2026/5/15
 */

#pragma once
#include <vector>
#include "cache.h"
#include "../../types.h"

class MemoryInterface;
enum class CacheState;
struct cacheline;
struct CacheResult;
class CacheInterface;
class Cache;
enum class CacheReplacePolicy;
enum class CacheWritePolicy;
enum class MemoryAccessType;

enum class CacheState {
    idle,
    busy,
};

enum class CacheReplacePolicy {
    LRU,
};

enum class CacheWritePolicy {
    WriteThrough,
    WriteBack,
};

enum class MemoryAccessType {
    Read,
    Write,
};

struct CacheRequest {
    u32 address;
    u32 value;
    MemoryAccessType accessType;
};

struct CacheResult {
    bool hit = false;
    u32 value = 0;
};

struct cacheline {
    bool valid = false;
    bool dirty = false;
    u32 tag{};
    u8 data[cacheline_size] = {};
};

class CacheInterface {
public:
    ~CacheInterface() = default;
private:
    virtual bool busy() const = 0;
    virtual CacheResult access(CacheRequest request) = 0;
};

class Cache : public CacheInterface {
public:
    Cache(u32 size, u32 associativity, CacheReplacePolicy replacePolicy, CacheWritePolicy writePolicy, MemoryInterface* memory);

    void tick();
    void reset();
    bool busy() const override;
    CacheResult access(CacheRequest request) override;

private:
    MemoryInterface* memory;

    CacheRequest pending_request{};
    u32 remaining_cycles = 0;

    CacheState state = CacheState::idle;

    std::vector<std::vector<cacheline>> sets;

    u32 size = 0;
    u32 associativity = 1;
    u32 line_size = cacheline_size;
    u32 set_count = size / (line_size * associativity);

    CacheReplacePolicy replacePolicy = CacheReplacePolicy::LRU;
    CacheWritePolicy writePolicy = CacheWritePolicy::WriteThrough;

    u32 tag_mask;
    u32 set_mask;
    u32 offset_mask;
};

