// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.
/**
 * @file
 *
 * This file implements optimizations for LLVMIR.
 */

#include "CJNative/IROptimizer.h"

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include "Utils/CGCommonDef.h"

using namespace Cangjie;
using namespace CodeGen;

// If @param inst is in a loop, return the inner most loop it lives in. Otherwise, return nullptr.
llvm::Loop* LICMForVtableLookup::GetMostInnerLoopFor(const llvm::Instruction* inst)
{
    auto func = inst->getFunction();
    llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>* loopInfo;
    // Try to get the result from the cache first.
    if (funcDTAndLoopInfo) {
        loopInfo = funcDTAndLoopInfo->loopInfo.get();
    } else {
        auto dtUPtr = std::make_unique<llvm::DominatorTree>(*const_cast<llvm::Function*>(func));
        auto loopInfoUPtr = std::make_unique<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>>();
        loopInfoUPtr->analyze(*dtUPtr);
        loopInfo = loopInfoUPtr.get();
        // Cache the analysis result to reduce redundant calculation.
        funcDTAndLoopInfo = std::make_unique<DTAndLoopInfo>(std::move(dtUPtr), std::move(loopInfoUPtr));
    }

    return loopInfo->getLoopFor(inst->getParent());
}

void LICMForVtableLookup::TryHoistLookupInstructions(
    const std::pair<std::pair<llvm::Value*, llvm::Instruction*>, llvm::Loop*>& lookupInsts,
    const llvm::MemorySSA& mSSA) const
{
    auto objVal = lookupInsts.first.first;
    auto loadInst = llvm::dyn_cast<llvm::LoadInst>(objVal);
    // We cannot move the global instance VTable/ITable lookup outside the loop since
    // functions can be executed concurrently and the global memory may be redefined
    // by other functions.
    // Also, for now, we don't optimize for the object which is not loaded from an
    // allocation. May consider supporting this in the future.
    if (loadInst &&
        (llvm::isa<llvm::GlobalVariable>(loadInst->getPointerOperand()) ||
            !llvm::isa<llvm::AllocaInst>(loadInst->getPointerOperand()))) {
        return;
    }
    auto loop = lookupInsts.second;
    auto lookupInst = lookupInsts.first.second;
    llvm::Loop* niceLoop = nullptr; // The loop in which the `objVal` is invariant
    llvm::Instruction* insertPt = nullptr;
    // Case 1: the address used in vtable-lookup is loaded from a local variable,
    // a simple example is given:
    // func foo() {
    //     var a = classA(1)
    //     for (i in 0..10) {       // loop-1
    //       a = classA(2)          // last store-instruction for `a`
    //       //...
    //       for (j in 0..10) {     // loop-2
    //         a.virtualCall()      // here exists a vtable-lookup instruction
    //       }
    //     }
    // }
    if (loadInst != nullptr) {
        auto loadMA = mSSA.getMemoryAccess(loadInst);
        auto use = llvm::cast<llvm::MemoryUse>(loadMA);
        // For case 1, we need to find the last store-instruction of the local variable.
        if (auto def = llvm::dyn_cast<llvm::MemoryDef>(use->getDefiningAccess());
            def && def->getMemoryInst() && llvm::isa<llvm::StoreInst>(def->getMemoryInst())) {
            auto inst = def->getMemoryInst();
            // Find the outermost loop(name it niceLoop) that doesn't contain the store-instruction.
            // As for the given example, `niceLoop` is loop-2.
            while (loop && !loop->contains(inst)) {
                niceLoop = loop;
                loop = loop->getParentLoop();
            }
            // If the store-instruction is in the same loop with vtable-lookup instruction,
            // we won't do anything.
            if (!niceLoop) {
                return;
            }
            // Use the same block with store instruction as the coarse lift position.
            insertPt = inst->getParent()->getTerminator();
        }
    } else if (llvm::dyn_cast<llvm::Argument>(objVal)) {
        // Case 2: the address used in vtable-lookup is from an argument,
        // a simple example is given:
        // func foo(a: classA) {
        //     for (i in 0..10) {       // loop-1
        //       for (j in 0..10) {     // loop-2
        //         a.virtualCall()      // here exists a vtable-lookup instruction
        //       }
        //     }
        // }
        // In this case, since semantically it is guaranteed the argument won't
        // be re-assigned, it is always a loop invariant variable.
        // Thus we can hoist the vtable-lookup instruction outside the outermost
        // loop.
        // As for the given example, `niceLoop` is loop-1.
        niceLoop = loop->getOutermostLoop();
        // Use the entry block as the coarse lift position.
        insertPt = lookupInst->getParent()->getParent()->getEntryBlock().getTerminator();
    }
    if (!niceLoop) {
        return;
    }
    // Update the lift position of vtable-lookup, make it closer to where the result is used.
    if (auto preHeader = niceLoop->getLoopPreheader(); preHeader) {
        insertPt = preHeader->getTerminator();
    }

    CJC_ASSERT(insertPt && "Wrong analysis result for LICMForVtableLookup");

    // Hoist the instructions.
    // If `loadInst` has already dominates `insertPt`, it doesn't need to
    // be moved further (because this moving will cause it to sink).
    if (loadInst && !funcDTAndLoopInfo->dt->dominates(loadInst, insertPt)) {
        loadInst->moveBefore(insertPt);
        loadInst->setDebugLoc(llvm::DebugLoc());
    }
    lookupInst->moveBefore(insertPt);
    lookupInst->setDebugLoc(llvm::DebugLoc());
}

void LICMForVtableLookup::MoveLoopInvariantVtableLookups(llvm::Function* function)
{
    auto dataLayout = function->getParent()->getDataLayout();
    std::vector<std::pair<std::pair<llvm::Value*, llvm::Instruction*>, llvm::Loop*>> instsWaitingMotion;
    // Collect the instruction groups used to look up the VTable/ITable.
    for (auto it = llvm::inst_begin(function); it != llvm::inst_end(function); ++it) {
        if (!it->hasMetadata(VTABLE_LOOKUP)) {
            continue;
        }
        if (auto loop = GetMostInnerLoopFor(&*it); loop) {
            auto ci = llvm::cast<llvm::CallInst>(&*it);
            // If the first argument of `ci` is not an instruction then it's an argument of function.
            llvm::Value* objectVal = *ci->arg_begin();
            if (cgCtx.IsNullableReference(objectVal)) {
                continue;
            }
            // The first one in the inner pair indicates the instruction for obtaining the instance
            // whose VTable/ITable needs to be queried.
            instsWaitingMotion.push_back(std::make_pair(std::make_pair(objectVal, &*it), loop));
        }
        // Erase the temporary metadata.
        it->setMetadata(VTABLE_LOOKUP, nullptr);
    }
    // If nothing need to be optimized, return.
    if (instsWaitingMotion.empty()) {
        return;
    }

    // Static-Single-Assignment analysis.
    auto tli = llvm::TargetLibraryInfo(llvm::TargetLibraryInfoImpl(), function);
    llvm::AAResults aaResults(tli);
    auto tti = llvm::TargetTransformInfo(dataLayout);
    llvm::AssumptionCache ac(*function, &tti);
    llvm::DominatorTree* dt = funcDTAndLoopInfo->dt.get();
    llvm::BasicAAResult baaResult(dataLayout, *function, tli, ac, dt);
    aaResults.addAAResult(baaResult);
    auto mSSA = llvm::MemorySSA(*function, &aaResults, dt);
    mSSA.ensureOptimizedUses();

    // Do the motion.
    for (auto lookupInsts : instsWaitingMotion) {
        TryHoistLookupInstructions(lookupInsts, mSSA);
    }
}
