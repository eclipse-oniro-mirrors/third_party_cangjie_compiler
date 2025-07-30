// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * MockSupportManager is the global manager to prepare classes for further possible mocking.
 */

#ifndef CANGJIE_SEMA_MOCK_SUPPORT_MANAGER_H
#define CANGJIE_SEMA_MOCK_SUPPORT_MANAGER_H

#include "cangjie/Sema/MockUtils.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Mangle/BaseMangler.h"

namespace Cangjie {

class MockSupportManager {
public:
    explicit MockSupportManager(TypeManager& typeManager, const Ptr<MockUtils> mockUtils);
    static bool IsDeclOpenToMock(const AST::Decl& decl);
    static bool DoesClassLikeSupportMocking(AST::ClassLikeDecl& classLikeToCheck);
    static void MarkNodeMockSupportedIfNeeded(AST::Node& node);
    static bool NeedToSearchCallsToReplaceWithAccessors(AST::Node& node);
    void GenerateSpyCallMarker(AST::Package& package);
    void GenerateAccessors(AST::Decl& decl);
    void WriteGeneratedMockDecls();
    Ptr<AST::Expr> ReplaceExprWithAccessor(
        AST::Expr& originalExpr, bool isInConstructor, bool isSubMemberAccess = false);
    void PrepareStaticDecls(std::vector<Ptr<AST::Decl>> decls);
    void CollectStaticDeclsToPrepare(AST::Decl& decl, std::vector<Ptr<AST::Decl>>& decls);
    void PrepareToSpy(AST::Decl& decl);

private:
    TypeManager& typeManager;
    Ptr<MockUtils> mockUtils;
    std::set<OwnedPtr<AST::Decl>> generatedMockDecls;
    std::unordered_map<Ptr<AST::Decl>, Ptr<AST::VarDecl>> genericMockVarsDecls;

    static void MakeOpenToMockIfNeeded(AST::Decl& decl);
    static void MarkMockAccessorWithAttributes(AST::Decl& decl);
    bool IsMemberAccessOnThis(const AST::MemberAccess& memberAccess) const;
    OwnedPtr<AST::FuncDecl> GenerateFuncAccessor(AST::FuncDecl& methodDecl) const;
    OwnedPtr<AST::PropDecl> GeneratePropAccessor(AST::PropDecl& propDecl) const;
    std::vector<OwnedPtr<AST::Node>> GenerateFieldGetterAccessorBody(
        AST::VarDecl& fieldDecl, AST::FuncBody& funcBody, AccessorKind kind) const;
    std::vector<OwnedPtr<AST::Node>> GenerateFieldSetterAccessorBody(
        AST::VarDecl& fieldDecl, AST::FuncParam& setterParam, AST::FuncBody& funcBody, AccessorKind kind) const;
    OwnedPtr<AST::FuncDecl> CreateFieldAccessorDecl(
        const AST::VarDecl& fieldDecl, AST::FuncTy* accessorTy, AccessorKind kind) const;
    OwnedPtr<AST::FuncDecl> CreateForeignFunctionAccessorDecl(AST::FuncDecl& funcDecl) const;
    OwnedPtr<AST::FuncDecl> GenerateVarDeclAccessor(AST::VarDecl& fieldDecl, AccessorKind kind);
    OwnedPtr<AST::CallExpr> GenerateAccessorCallForField(const AST::MemberAccess& memberAccess, AccessorKind kind);
    Ptr<AST::Expr> ReplaceFieldGetWithAccessor(AST::MemberAccess& memberAccess, bool isInConstructor);
    Ptr<AST::Expr> ReplaceFieldSetWithAccessor(AST::AssignExpr& assignExpr, bool isInConstructor);
    Ptr<AST::Expr> ReplaceMemberAccessWithAccessor(AST::MemberAccess& memberAccess, bool isInConstructor);
    void TransformAccessorCallForMutOperation(
        AST::MemberAccess& originalMa, AST::Expr& replacedMa, AST::Expr& topLevelExpr);
    void ReplaceSubMemberAccessWithAccessor(
        const AST::MemberAccess& memberAccess, bool isInConstructor, const Ptr<AST::Expr> topLevelMutExpr = nullptr);
    Ptr<AST::Expr> ReplaceTopLevelVariableGetWithAccessor(AST::RefExpr& refExpr);
    OwnedPtr<AST::CallExpr> GenerateAccessorCallForTopLevelVariable(
        const AST::RefExpr& refExpr, AccessorKind kind);
    void GenerateVarDeclAccessors(AST::VarDecl& fieldDecl, AccessorKind getterKind, AccessorKind setterKind);
    void PrepareStaticDecl(AST::Decl& decl);
    std::vector<OwnedPtr<AST::MatchCase>> GenerateHandlerMatchCases(
        const AST::FuncDecl& funcDecl,
        OwnedPtr<AST::EnumPattern> optionFuncTyPattern, OwnedPtr<AST::RefExpr> handlerCallExpr);
    Ptr<AST::Decl> GenerateSpiedObjectVar(const AST::Decl& decl);
    void GenerateSpyCallHandler(AST::FuncDecl& funcDecl, AST::Decl& spiedObjectDecl);
};
} // namespace Cangjie

#endif // CANGJIE_SEMA_MOCK_SUPPORT_MANAGER_H
