// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CGCPOINTERTYPE_H
#define CANGJIE_CGCPOINTERTYPE_H

#include "Base/CGTypes/CGType.h"

namespace Cangjie {
namespace CodeGen {
class CGCPointerType : public CGType {
    friend class CGTypeMgr;

protected:
    llvm::Type* GenLLVMType() override;
    void GenContainedCGTypes() override;

private:
    CGCPointerType() = delete;

    explicit CGCPointerType(CGModule& cgMod, CGContext& cgCtx, const CHIR::CPointerType& chirType)
        : CGType(cgMod, cgCtx, chirType)
    {
    }

    llvm::Constant* GenSuperOfTypeInfo() override;
    llvm::Constant* GenTypeArgsNumOfTypeInfo() override;
    llvm::Constant* GenTypeArgsOfTypeInfo() override;

    void CalculateSizeAndAlign() override
    {
        size = sizeof(void*);
        align = alignof(void*);
    };
};
} // namespace CodeGen
} // namespace Cangjie

#endif // CANGJIE_CGCPOINTERTYPE_H
