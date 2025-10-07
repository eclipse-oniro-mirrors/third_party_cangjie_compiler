// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares Block Scope for codegen.
 */

#ifndef CANGJIE_BLOCKSCOPEIMPL_H
#define CANGJIE_BLOCKSCOPEIMPL_H

#include "llvm/IR/Value.h"

#include "IRBuilder.h"

namespace Cangjie {
namespace CHIR {
class Func;
}
namespace CodeGen {
class IRBuilder2;

class CodeGenBlockScope {
public:
    CodeGenBlockScope(IRBuilder2& irBuilder, llvm::BasicBlock* bb)
        : irBuilder(irBuilder), oldBb(irBuilder.GetInsertBlock())
    {
        irBuilder.SetInsertPoint(bb);
    }

    CodeGenBlockScope(IRBuilder2& irBuilder, const CHIR::Block& chirBlock)
            : irBuilder(irBuilder), oldBb(irBuilder.GetInsertBlock())
    {
        irBuilder.SetInsertPoint(irBuilder.GetCGModule().GetMappedBB(&chirBlock));
        irBuilder.SetInsertCGFunction(*irBuilder.GetCGModule().GetOrInsertCGFunction(chirBlock.GetParentFunc()));
    }

    ~CodeGenBlockScope()
    {
        irBuilder.SetInsertPoint(oldBb);
    }

private:
    IRBuilder2& irBuilder;
    llvm::BasicBlock* oldBb;
};

class CodeGenFunctionScope {
public:
    CodeGenFunctionScope(IRBuilder2& irBuilder, llvm::Function* function)
        : refFunc(nullptr), block(irBuilder, &function->getEntryBlock())
    {
        oldFunc = nullptr;
    }

    ~CodeGenFunctionScope()
    {
        if (refFunc) {
            *refFunc = oldFunc;
        }
    }

private:
    const CHIR::Func** refFunc;
    CodeGenBlockScope block;
    const CHIR::Func* oldFunc;
};

class CodeGenUnwindBlockScope {
public:
    CodeGenUnwindBlockScope(CGModule& cgMod, llvm::BasicBlock* unwindBlock) : cgMod(cgMod)
    {
        cgMod.GetCGContext().PushUnwindBlockStack(unwindBlock);
    }

    ~CodeGenUnwindBlockScope()
    {
        cgMod.GetCGContext().PopUnwindBlockStack();
    }

private:
    CGModule& cgMod;
};

} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_BLOCKSCOPEIMPL_H
