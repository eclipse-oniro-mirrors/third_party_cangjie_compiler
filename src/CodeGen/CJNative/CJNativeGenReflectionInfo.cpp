// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Implements the generator for llvm reflection info.
 */

#include "CJNative/CJNativeReflectionInfo.h"

#include "CJNative/CJNativeMetadata.h"

using namespace Cangjie;
using namespace CodeGen;

void CJNativeReflectionInfo::Gen() const
{
    // Generate metadata for packages, classes, interfaces, structs, enums, global functions, and global variables.
    CGMetadata(cgMod, subCHIRPkg)
        .Needs(MetadataKind::PKG_METADATA)
        ->Needs(MetadataKind::CLASS_METADATA)
        ->Needs(MetadataKind::STRUCT_METADATA)
        ->Needs(MetadataKind::ENUM_METADATA)
        ->Needs(MetadataKind::GF_METADATA)
        ->Needs(MetadataKind::GV_METADATA)
        ->Gen();
}
