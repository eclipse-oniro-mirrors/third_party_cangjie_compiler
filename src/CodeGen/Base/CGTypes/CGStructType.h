// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CGSTRUCTTYPE_H
#define CANGJIE_CGSTRUCTTYPE_H

#include "Base/CGTypes/CGCustomType.h"
#include "cangjie/CHIR/CHIRCasting.h"

namespace Cangjie {
namespace CodeGen {
class CGStructType : public CGCustomType {
    friend class CGTypeMgr;

protected:
    llvm::Type* GenLLVMType() override;
    void GenContainedCGTypes() override;

private:
    CGStructType() = delete;

    explicit CGStructType(CGModule& cgMod, CGContext& cgCtx, const CHIR::Type& chirType)
        : CGCustomType(cgMod, cgCtx, chirType)
    {
    }

    llvm::Constant* GenFieldsNumOfTypeInfo() override;
    llvm::Constant* GenFieldsOfTypeInfo() override;
    void CalculateSizeAndAlign() override;
};
} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_CGSTRUCTTYPE_H
