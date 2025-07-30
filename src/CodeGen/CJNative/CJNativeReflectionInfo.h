// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the generator for llvm reflection info.
 */

#ifndef CANGJIE_CJNATIVE_REFLECTION_INFO_GEN_H
#define CANGJIE_CJNATIVE_REFLECTION_INFO_GEN_H

#include "CGModule.h"

namespace Cangjie {
namespace CodeGen {
struct SubCHIRPackage;
class CJNativeReflectionInfo {
public:
    CJNativeReflectionInfo(CGModule& cgMod, const SubCHIRPackage& subCHIRPackage)
        : cgMod(cgMod), subCHIRPkg(subCHIRPackage)
    {
    }
    void Gen() const;

private:
    CGModule& cgMod;
    const SubCHIRPackage& subCHIRPkg;
};
} // namespace CodeGen
} // namespace Cangjie
#endif
