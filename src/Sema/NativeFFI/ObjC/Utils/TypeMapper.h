// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares type mappings Cangjie <-> Objective-C helper
 */

#ifndef CANGJIE_SEMA_OBJ_C_UTILS_TYPE_MAPPER_H
#define CANGJIE_SEMA_OBJ_C_UTILS_TYPE_MAPPER_H

#include "cangjie/AST/Node.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/SafePointer.h"
#include "InteropLibBridge.h"

namespace Cangjie::Interop::ObjC {
class TypeMapper {
public:
    explicit TypeMapper(InteropLibBridge& bridge, TypeManager& typeManager)
        : bridge(bridge), typeManager(typeManager)
    {
    }
    
    std::string Cj2ObjCForObjC(const AST::Ty& from) const;
    Ptr<AST::Ty> Cj2CType(Ptr<AST::Ty> cjty) const;
    static bool IsObjCCompatible(const AST::Ty& ty);
    static bool IsObjCMirror(const AST::Decl& decl);
    static bool IsObjCMirrorSubtype(const AST::Decl& decl);
    static bool IsObjCImpl(const AST::Decl& decl);
    static bool IsValidObjCMirror(const AST::Ty& ty);
    static bool IsObjCMirrorSubtype(const AST::Ty& ty);
    static bool IsObjCImpl(const AST::Ty& ty);
    static bool IsObjCMirror(const AST::Ty& ty);
    static bool IsObjCPointer(const AST::Decl& decl);
    static bool IsObjCPointer(const AST::Ty& ty);
private:
    InteropLibBridge& bridge;
    TypeManager& typeManager;
};
} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_OBJ_C_UTILS_TYPE_MAPPER_H
