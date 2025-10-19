// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CGPRIMITIVETYPE_H
#define CANGJIE_CGPRIMITIVETYPE_H

#include "Base/CGTypes/CGType.h"
#include "CGContext.h"
#include "cangjie/CHIR/Type/Type.h"

namespace Cangjie {
namespace CodeGen {
class CGPrimitiveType : public CGType {
    friend class CGTypeMgr;

protected:
    llvm::Type* GenLLVMType() override;
    void GenContainedCGTypes() override;

private:
    CGPrimitiveType() = delete;
    explicit CGPrimitiveType(CGModule& cgMod, CGContext& cgCtx, const CHIR::Type& chirType)
        : CGType(cgMod, cgCtx, chirType, CGTypeKind::CG_PRIMITIVE)
    {
        CJC_ASSERT(
            chirType.GetTypeKind() >= CHIR::Type::TYPE_INT8 && chirType.GetTypeKind() <= CHIR::Type::TYPE_VOID);
    }

    void CalculateSizeAndAlign() override;
};
} // namespace CodeGen
} // namespace Cangjie

#endif // CANGJIE_CGPRIMITIVETYPE_H
