// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file defines classes for optimizing IR.
 */

#ifndef CANGJIE_IROPTIMIZER2_H
#define CANGJIE_IROPTIMIZER2_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"

#include "CGModule.h"

namespace Cangjie {
namespace CodeGen {
using DominatorTreeUPtr = std::unique_ptr<llvm::DominatorTree>;
using LoopInfoBaseUPtr = std::unique_ptr<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>>;

class LICMForVtableLookup {
public:
    explicit LICMForVtableLookup(const CGContext& cgCtx, llvm::Function& function) : cgCtx(cgCtx), function(function)
    {
    }
    void Run()
    {
        MoveLoopInvariantVtableLookups(&function);
    }

private:
    struct DTAndLoopInfo {
        DominatorTreeUPtr dt{nullptr};
        LoopInfoBaseUPtr loopInfo{nullptr};
        DTAndLoopInfo() = delete;
        DTAndLoopInfo(DominatorTreeUPtr dt, LoopInfoBaseUPtr loopInfo)
            : dt(std::move(dt)), loopInfo(std::move(loopInfo))
        {
        }
    };
    // If @param inst is in a loop, return the innermost loop it lives in. Otherwise, return nullptr.
    llvm::Loop* GetMostInnerLoopFor(const llvm::Instruction* inst);
    void TryHoistLookupInstructions(
        const std::pair<std::pair<llvm::Value*, llvm::Instruction*>, llvm::Loop*>& lookupInsts,
        const llvm::MemorySSA& mSSA) const;
    void MoveLoopInvariantVtableLookups(llvm::Function* function);

    std::unique_ptr<DTAndLoopInfo> funcDTAndLoopInfo = nullptr; // To speedup, cache some data.

    const CGContext& cgCtx;
    llvm::Function& function;
};
} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_IROPTIMIZER2_H
