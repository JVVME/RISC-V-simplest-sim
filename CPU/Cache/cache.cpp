//
// Created by zhang on 2026/5/15.
//

#include "cache.h"

#include "../../Memory/memory.h"

Cache::Cache(u32 size, u32 associativity,
             CacheReplacePolicy replacePolicy, CacheWritePolicy writePolicy, MemoryInterface* memory)
    : size(size),
      associativity(associativity),
      replacePolicy(replacePolicy),
      writePolicy(writePolicy),
    memory(memory){
    // 计算 set 数量
    set_count = size / (line_size * associativity);

    // 初始化 sets 容器，每个 set 包含 associativity 个 cacheline
    sets.resize(set_count);
    for (auto &set : sets) {
        set.resize(associativity);
    }

    // 计算 offset bits mask
    // offset_bits = log2(line_size)
    u32 offset_bits = 0;
    u32 tmp = line_size;
    while (tmp > 1) {
        tmp >>= 1;
        offset_bits++;
    }
    offset_mask = (1u << offset_bits) - 1;

    // 计算 index bits mask
    // set_count 必须是 2^n
    u32 index_bits = 0;
    tmp = set_count;
    while (tmp > 1) {
        tmp >>= 1;
        index_bits++;
    }
    set_mask = ((1u << index_bits) - 1) << offset_bits;

    // tag mask = 剩下的高位
    tag_mask = ~(offset_mask | set_mask);
}

void Cache::tick() {
}

void Cache::reset() {
}

bool Cache::busy() const {
    return this->state == CacheState::busy;
}

CacheResult Cache::access(CacheRequest request) {

    CacheResult result{};

    //--------------------------------
    // cache busy
    //--------------------------------

    if (state == CacheState::busy) {
        result.hit = false;
        return result;
    }

    //--------------------------------
    // address decomposition
    //--------------------------------

    u32 offset_bits = std::countr_zero(line_size);
    u32 index_bits = std::countr_zero(set_count);

    u32 offset =
        request.address & offset_mask;

    u32 set_index =
        (request.address & set_mask)
        >> offset_bits;

    u32 tag =
        request.address
        >> (offset_bits + index_bits);

    //--------------------------------
    // lookup
    //--------------------------------

    auto &set = sets[set_index];

    for (auto &line : set) {

        if (line.valid &&
            line.tag == tag) {

            //--------------------------------
            // HIT
            //--------------------------------
            result.hit = true;

            //--------------------------------
            // READ
            //--------------------------------
            if (request.accessType == MemoryAccessType::Read) {

                result.value = line.data[offset];
            }

            //--------------------------------
            // WRITE
            //--------------------------------
            else {
                line.data[offset] = request.value;

                //--------------------------------
                // write-back
                //--------------------------------
                if (writePolicy ==
                    CacheWritePolicy::WriteBack) {

                    line.dirty = true;
                }

                //--------------------------------
                // write-through
                //--------------------------------
                else {
                    memory->store_byte(request.address, result.value);
                }
            }

            return result;
        }
    }

    //--------------------------------
    // MISS
    //--------------------------------

    result.hit = false;

    state = CacheState::busy;

    pending_request = request;

    remaining_cycles =
        block_cache_miss_latency;

    return result;
}
