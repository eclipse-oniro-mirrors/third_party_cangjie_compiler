// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares core context for the core handlers of Cangjie <-> Objective-C interopability.
 */

#ifndef CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT
#define CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT

#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"
#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/InteropLibBridge.h"
#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "NativeFFI/ObjC/Utils/TypeMapper.h"

namespace Cangjie::Interop::ObjC {

struct InteropContext {
    explicit InteropContext(
        AST::Package& pkg, TypeManager& typeManager, ImportManager& importManager, DiagnosticEngine& diag,
        const BaseMangler& mangler,
        const std::string& cjLibOutputPath)
        : pkg(pkg), diag(diag), typeManager(typeManager), importManager(importManager), bridge(importManager, diag),
          typeMapper(bridge, typeManager), mangler(mangler), nameGenerator(mangler),
          factory(bridge, typeManager, nameGenerator, typeMapper, importManager),
          cjLibOutputPath(cjLibOutputPath)
    {
    }

    AST::Package& pkg;
    std::vector<Ptr<AST::ClassLikeDecl>> mirrors;
    std::vector<Ptr<AST::ClassDecl>> impls;
    std::vector<OwnedPtr<AST::Decl>> genDecls;

    DiagnosticEngine& diag;
    TypeManager& typeManager;
    ImportManager& importManager;
    InteropLibBridge bridge;
    TypeMapper typeMapper;
    const BaseMangler& mangler;
    NameGenerator nameGenerator;
    ASTFactory factory;
    const std::string& cjLibOutputPath;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT
