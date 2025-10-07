// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CGBOXTYPE_H
#define CANGJIE_CGBOXTYPE_H

#include "Base/CGTypes/CGType.h"

namespace Cangjie {
namespace CodeGen {
class CGBoxType : public CGType {
    friend class CGTypeMgr;

protected:
    llvm::Type* GenLLVMType() override;
    void GenContainedCGTypes() override;

private:
    CGBoxType() = delete;

    explicit CGBoxType(CGModule& cgMod, CGContext& cgCtx, const CHIR::BoxType& chirType);

    void CalculateSizeAndAlign() override;
};
} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_CGBOXTYPE_H
