// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares class for auto boxing extend and option.
 */
#ifndef CANGJIE_SEMA_AUTO_BOX_H
#define CANGJIE_SEMA_AUTO_BOX_H

#include <memory>

#include "cangjie/AST/ASTContext.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie {
class AutoBoxing {
public:
    AutoBoxing(TypeManager& typeManager, ImportManager& importManager, ASTContext& ctx)
        : typeManager(typeManager), importManager(importManager), ctx(ctx)
    {
    }
    ~AutoBoxing() = default;

    void AddExtendBox(AST::Package& pkg);
    void AddOptionBox(AST::Package& pkg);

private:
    /** Boxing functions for option box. */
    void TryOptionBox(AST::EnumTy& target, AST::Expr& expr);
    bool NeedBoxOption(AST::Ty& child, AST::Ty& target);
    AST::VisitAction AddOptionBoxHandleReturnExpr(const AST::ReturnExpr& re);
    AST::VisitAction AddOptionBoxHandleVarDecl(const AST::VarDecl& vd);
    AST::VisitAction AddOptionBoxHandleAssignExpr(const AST::AssignExpr& ae);
    AST::VisitAction AddOptionBoxHandleCallExpr(AST::CallExpr& ce);
    AST::VisitAction AddOptionBoxHandleIfExpr(const AST::IfExpr& ie);
    AST::VisitAction AddOptionBoxHandleTryExpr(AST::TryExpr& te);
    AST::VisitAction AddOptionBoxHandleArrayLit(AST::ArrayLit& lit);
    AST::VisitAction AddOptionBoxHandleMatchExpr(AST::MatchExpr& me);
    AST::VisitAction AddOptionBoxHandleArrayExpr(AST::ArrayExpr& ae);
    AST::VisitAction AddOptionBoxHandleTupleList(const AST::TupleLit& tl);

    /** Boxing functions for extend box. */
    AST::VisitAction AutoBoxCallExpr(AST::CallExpr& ce);
    AST::VisitAction AutoBoxIfExpr(AST::IfExpr& ie);
    AST::VisitAction AutoBoxWhileExpr(const AST::WhileExpr& we);
    /// \return whether a subpattern is desugared
    bool AutoBoxCondition(AST::Expr& condition);
    AST::VisitAction AutoBoxArrayLit(AST::ArrayLit& lit);
    AST::VisitAction AutoBoxMatchExpr(AST::MatchExpr& me);
    void AutoBoxBlock(AST::Block& block, AST::Ty& ty);
    void AddOptionBoxHandleBlock(AST::Block& block, AST::Ty& ty);
    bool AutoBoxOrUnboxTypePatterns(AST::TypePattern& typePattern, AST::Ty& selectorTy);
    bool AutoBoxOrUnboxPatterns(AST::Pattern& pattern, AST::Ty& selectorTy);
    void UnBoxingTypePattern(AST::TypePattern& typePattern, const AST::Ty& selectorTy);
    void AutoBoxTypePattern(AST::TypePattern& typePattern, AST::Ty& selectorTy);
    AST::VisitAction AutoBoxTryExpr(AST::TryExpr& te);
    AST::VisitAction AutoBoxTupleLit(AST::TupleLit& tl);
    AST::VisitAction AutoBoxArrayExpr(AST::ArrayExpr& ae);
    AST::VisitAction AutoBoxAssignExpr(AST::AssignExpr& ae);
    AST::VisitAction AutoBoxVarDecl(AST::VarDecl& vd);
    AST::VisitAction AutoBoxReturnExpr(AST::ReturnExpr& re);
    OwnedPtr<AST::CallExpr> AutoBoxingCallExpr(OwnedPtr<AST::Expr>&& expr, const AST::ClassDecl& cd) const;
    OwnedPtr<AST::ClassDecl> AutoBoxedType(
        const std::string& packageName, AST::Ty& beBoxedType, AST::ClassDecl& base);
    OwnedPtr<AST::ClassDecl> AutoBoxedBaseType(AST::Ty& beBoxedType);
    void AddSuperClassForBoxedType(AST::ClassDecl& cd, const AST::Ty& beBoxedType);
    std::string GetAutoBoxedTypeName(const AST::Ty& beBoxedType, bool isBaseBox = true);
    Ptr<AST::ClassDecl> GetBoxedBaseDecl(AST::File& curFile, AST::Ty& ty, const AST::Ty& iTy);
    Ptr<AST::ClassDecl> GetBoxedDecl(AST::File& curFile, AST::Ty& ty, const AST::Ty& iTy);
    void CollectExtendedInterface(
        AST::Ty& beBoxedType, const OwnedPtr<AST::ClassDecl>& cd, AST::VarDecl& varDecl);
    void CollectSpecifiedInheritedType(
        const OwnedPtr<AST::ClassDecl>& cd, const std::string& pkgName, const std::string& typeName);
    void CollectExtendedInterfaceHelper(
        const OwnedPtr<AST::ClassDecl>& cd, AST::VarDecl& varDecl, const std::set<Ptr<AST::ExtendDecl>>& extends) const;
    void CreateConstructor(AST::Ty& beBoxedType, const OwnedPtr<AST::ClassDecl>& cd,
        OwnedPtr<AST::FuncParam> funcParam, OwnedPtr<AST::CallExpr> superCall);

    TypeManager& typeManager;
    ImportManager& importManager;
    ASTContext& ctx;
    BaseMangler mangler;  // Use for box decl's name mangling.
};
} // namespace Cangjie

#endif
