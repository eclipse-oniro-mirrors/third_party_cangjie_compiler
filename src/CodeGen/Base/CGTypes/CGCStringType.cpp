// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "Base/CGTypes/CGCStringType.h"

#include "CGContext.h"

namespace Cangjie::CodeGen {
llvm::Type* CGCStringType::GenLLVMType()
{
    if (llvmType) {
        return llvmType;
    }
    auto& llvmCtx = cgCtx.GetLLVMContext();
    llvmType = llvm::Type::getInt8PtrTy(llvmCtx);

    layoutType = llvm::StructType::getTypeByName(llvmCtx, "CString.Type");
    if (!layoutType) {
        layoutType = llvm::StructType::create(llvmCtx, {llvmType}, "CString.Type");
    }
    return llvmType;
}
} // namespace Cangjie::CodeGen
