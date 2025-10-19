// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CGGENERICTYPE_H
#define CANGJIE_CGGENERICTYPE_H

#include "Base/CGTypes/CGType.h"
#include "CGContext.h"
#include "cangjie/CHIR/Type/Type.h"

namespace Cangjie {
namespace CodeGen {
class CGGenericType : public CGType {
    friend class CGTypeMgr;

public:
    llvm::GlobalVariable* GetOrCreateTypeInfo() override;

private:
    CGGenericType() = delete;
    explicit CGGenericType(CGModule& cgMod, CGContext& cgCtx, const CHIR::Type& chirType)
        : CGType(cgMod, cgCtx, chirType)
    {
        CJC_ASSERT(chirType.GetTypeKind() == CHIR::Type::TYPE_GENERIC);
    }
    std::vector<CHIR::Type*> upperBounds;
    llvm::Type* GenLLVMType() override;
    void GenContainedCGTypes() override;
    void CalculateSizeAndAlign() override;
    llvm::Constant* GenUpperBoundsOfGenericType(std::string& uniqueName);
};
} // namespace CodeGen
} // namespace Cangjie

#endif // CANGJIE_CGGENERICTYPE_H
