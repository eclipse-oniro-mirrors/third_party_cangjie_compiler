// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares the entry of CHIR.
 */

#ifndef CANGJIE_CHIR_BUMPPTRALLOCATOR_H
#define CANGJIE_CHIR_BUMPPTRALLOCATOR_H

#include <cstdlib>
#include <vector>
#include "CheckUtils.h"

namespace Cangjie::CHIR {

class BumpPtrAllocator {
public:
    explicit BumpPtrAllocator(size_t size, size_t subMemPoolNum);

    ~BumpPtrAllocator()
    {
    }

    // Freeing all memory allocated so far.
    void Reset();

    // Apply for available memory space from the sub-memory pool.
    void* Allocate(size_t size, size_t memoryPoolIdx, size_t alignment = 4);

private:
    void InitializeMemoryPool(size_t size, size_t subMemPoolNum);

    // Returns the size of the available memory space in the sub-memory pool.
    size_t GetSubMemPoolAvailableSize(size_t memoryPoolIdx);

    // Returns the offset to the next integer that is greater than
    // or equal to addr and is a multiple of alignment.
    size_t OffsetToAlignedAddr(size_t size, size_t alignment) const;

    std::vector<void*> startPtrs; // Indicates the start address of all large memories.

    std::vector<void*> curPtrs; // The next free byte of all subpools.

    std::vector<void*> endPtrs; // End of all sub-pools.

    size_t newMemPoolSize; // Size of a new memory pool. The default value is one tenth of the size of the original
                           // sub-memory pool.

    size_t divFactor = 10;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_BUMPPTRALLOCATOR_H
