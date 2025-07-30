// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the entry of CHIR.
 */

#include "cangjie/Utils/BumpPtrAllocator.h"
#include <securec.h>

using namespace Cangjie::CHIR;

BumpPtrAllocator::BumpPtrAllocator(size_t size, size_t subMemPoolNum)
{
    curPtrs.resize(subMemPoolNum);
    endPtrs.resize(subMemPoolNum);
    InitializeMemoryPool(size, subMemPoolNum);
}

// Freeing all memory allocated so far.
void BumpPtrAllocator::Reset()
{
    for (size_t i = 0; i < startPtrs.size(); i++) {
        std::free(startPtrs[i]);
    }
}

// Apply for available memory space from the sub-memory pool.
void* BumpPtrAllocator::Allocate(size_t size, size_t memoryPoolIdx, size_t alignment)
{
    size_t adjustment = OffsetToAlignedAddr(size, alignment);
    CJC_ASSERT(adjustment + size >= size && "adjustment + size must not overflow");

    size_t sizeToAllocate = adjustment + size;

    CJC_NULLPTR_CHECK(curPtrs[memoryPoolIdx]);
    // The space of the sub-memory pool is sufficient.
    if (sizeToAllocate <= GetSubMemPoolAvailableSize(memoryPoolIdx)) {
        void* res = curPtrs[memoryPoolIdx];
        curPtrs[memoryPoolIdx] =
            reinterpret_cast<void*>((sizeToAllocate + reinterpret_cast<size_t>(curPtrs[memoryPoolIdx])));
        return res;
    }
    // When the space of the sub-memory pool is insufficient, a new small memory space needs to be applied for.
    if (sizeToAllocate > newMemPoolSize) {
        CJC_ASSERT(sizeToAllocate > newMemPoolSize && "Unable to allocate memory!");
        return nullptr;
    }
    void* newMemPool = malloc(newMemPoolSize);
    CJC_NULLPTR_CHECK(newMemPool);
    memset_s(newMemPool, newMemPoolSize, 0, newMemPoolSize);
    startPtrs.emplace_back(newMemPool);
    curPtrs[memoryPoolIdx] = reinterpret_cast<void*>((sizeToAllocate + reinterpret_cast<size_t>(newMemPool)));
    endPtrs[memoryPoolIdx] =
        reinterpret_cast<void*>((reinterpret_cast<size_t>(curPtrs[memoryPoolIdx]) + newMemPoolSize - 1));
    return newMemPool;
}

void BumpPtrAllocator::InitializeMemoryPool(size_t size, size_t subMemPoolNum)
{
    void* curPtr = malloc(size);
    CJC_NULLPTR_CHECK(curPtr);
    memset_s(curPtr, size, 0, size);

    startPtrs.emplace_back(curPtr);
    size_t subMemPoolSize = size / subMemPoolNum;
    newMemPoolSize = subMemPoolSize / divFactor;
    for (size_t i = 0; i < subMemPoolNum; i++) {
        curPtrs[i] = reinterpret_cast<void*>(reinterpret_cast<size_t>(curPtr) + i * subMemPoolSize);
        endPtrs[i] = reinterpret_cast<void*>(reinterpret_cast<size_t>(curPtrs[i]) + subMemPoolSize - 1);
    }
}

// Returns the size of the available memory space in the sub-memory pool.
size_t BumpPtrAllocator::GetSubMemPoolAvailableSize(size_t memoryPoolIdx)
{
    return (reinterpret_cast<size_t>(endPtrs[memoryPoolIdx]) - reinterpret_cast<size_t>(curPtrs[memoryPoolIdx])) + 1;
}

// Returns the offset to the next integer that is greater than
// or equal to addr and is a multiple of alignment.
size_t BumpPtrAllocator::OffsetToAlignedAddr(size_t size, size_t alignment) const
{
    // The following line is equivalent to `(value + alignment - 1) / alignment * alignment - value`.

    // The division followed by a multiplication can be thought of as a right
    // shift followed by a left shift which zeros out the extra bits produced in
    // the bump; `~(alignment - 1)` is a mask where all those bits being zeroed out
    // are just zero.
    return ((size + alignment - 1) & ~(alignment - 1U)) - size;
}
