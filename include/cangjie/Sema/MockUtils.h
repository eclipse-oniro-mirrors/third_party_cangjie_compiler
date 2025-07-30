// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the TypeManager related classes, which manages all types.
 */

#ifndef CANGJIE_SEMA_MOCK_UTILS_H
#define CANGJIE_SEMA_MOCK_UTILS_H

#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie {

enum class AccessorKind : uint8_t {
    FIELD_GETTER,
    FIELD_SETTER,
    PROP,
    PROP_GETTER,
    PROP_SETTER,
    METHOD,
    TOP_LEVEL_FUNCTION,
    STATIC_METHOD,
    STATIC_PROP_GETTER,
    STATIC_PROP_SETTER,
    STATIC_FIELD_GETTER,
    STATIC_FIELD_SETTER,
    TOP_LEVEL_VARIABLE_GETTER,
    TOP_LEVEL_VARIABLE_SETTER
};

class MockUtils {
public:
    explicit MockUtils(
        ImportManager& importManager, TypeManager& typeManager,
        std::function<void(AST::Node& node)> instantiate,
        std::function<Ptr<AST::Decl>(AST::Decl&, const std::vector<Ptr<AST::Ty>>&, Ptr<AST::Ty>)> getInstantiatedDecl,
        std::function<std::unordered_set<Ptr<AST::Decl>>(AST::Decl& decl)> getInstantiatedDecls);
    static bool IsMockAccessor(const AST::Decl& decl);

private:
    ImportManager& importManager;
    TypeManager& typeManager;
    BaseMangler mangler;
    
    std::function<void(AST::Node& node)> instantiate;
    std::function<Ptr<AST::Decl>(
        AST::Decl& decl, const std::vector<Ptr<AST::Ty>>& instTys, Ptr<AST::Ty> baseTy)> getInstantiatedDecl;
    std::function<std::unordered_set<Ptr<AST::Decl>>(AST::Decl& decl)> getInstantiatedDecls;

    Ptr<AST::FuncDecl> getTypeForTypeParamDecl = nullptr;
    Ptr<AST::StructDecl> arrayDecl;
    Ptr<AST::StructDecl> stringDecl;
    Ptr<AST::EnumDecl> optionDecl;
    Ptr<AST::InheritableDecl> toStringDecl;
    Ptr<AST::FuncDecl> zeroValueDecl;
    
    static bool const isInstantiationEnabled;

    static bool IsMockAccessorRequired(const AST::Decl& decl);
    static std::string GetOriginalIdentifierOfMockAccessor(const AST::Decl& decl);
    static std::string GetOriginalIdentifierOfAccessor(const AST::FuncDecl& decl);
    static std::string BuildMockAccessorIdentifier(const AST::Decl& originalDecl, AccessorKind kind);
    static AccessorKind ComputeAccessorKind(const AST::FuncDecl& accessorDecl);
    static bool IsGetterForMutField(const AST::FuncDecl& accessorDecl);
    static std::string mockAccessorSuffix;
    static std::string spyObjVarName;
    static std::string spyCallMarkerVarName;
    static Ptr<AST::Decl> FindMockGlobalDecl(const AST::Decl& decl, const std::string& name);
    static void PrependFuncGenericSubst(
        const Ptr<AST::Generic> originalGeneric,
        const Ptr<AST::Generic> mockedGeneric,
        std::vector<TypeSubst>& classSubsts);
    static std::vector<TypeSubst> BuildGenericSubsts(const Ptr<AST::InheritableDecl> decl);
    static std::string GetForeignAccessorName(const AST::FuncDecl& decl);

    template <typename T> static OwnedPtr<T> CreateType(const Ptr<AST::Ty> ty)
    {
        auto type = MakeOwned<T>();
        type->ty = ty;
        return type;
    }

    Ptr<AST::Decl> FindAccessor(AST::ClassDecl& outerClass, const Ptr<AST::Decl> member,
        const std::vector<Ptr<AST::Ty>>& instTys, AccessorKind kind);
    Ptr<AST::Decl> FindAccessorForMemberAccess(const AST::MemberAccess& memberAccess,
        const Ptr<AST::Decl> resolvedMember, const std::vector<Ptr<AST::Ty>>& instTys, AccessorKind kind);
    Ptr<AST::FuncDecl> FindTopLevelAccessor(Ptr<AST::Decl> member, AccessorKind kind);
    OwnedPtr<AST::Expr> WrapCallTypeArgsIntoArray(const AST::Decl& decl);
    bool IsGeneratedGetter(AccessorKind kind);
    Ptr<AST::FuncDecl> FindAccessor(Ptr<AST::MemberAccess> ma, Ptr<AST::Decl> target, AccessorKind kind);
    void AddGenericIfNeeded(AST::Decl& originalDecl, AST::Decl& mockedDecl) const;
    OwnedPtr<AST::ArrayLit> WrapCallArgsIntoArray(const AST::FuncDecl& mockedFunc);
    Ptr<AST::Ty> GetInstantiatedTy(const Ptr<AST::Ty> ty, std::vector<TypeSubst>& typeSubsts);
    void GenerateGetTypeForTypeParamIntrinsic(AST::Package& pkg);
    OwnedPtr<AST::Expr> CreateGetTypeForTypeParameterCall(const Ptr<AST::GenericParamDecl> genericParam);
    std::string Mangle(const AST::Decl& decl) const;

    std::optional<std::unordered_set<Ptr<AST::Decl>>> TryGetInstantiatedDecls(AST::Decl& decl) const;
        OwnedPtr<AST::RefExpr> CreateRefExprWithInstTys(
        AST::Decl& target, const std::vector<Ptr<AST::Ty>>& instTys,
        const std::string& refName, AST::File& curFile) const;
    OwnedPtr<AST::RefExpr> CreateDeclBasedReferenceExpr(
        AST::Decl& target, const std::vector<Ptr<AST::Ty>>& instTys,
        const std::string& refName, AST::File& curFile
    ) const;
    OwnedPtr<AST::CallExpr> CreateZeroValue(Ptr<AST::Ty> ty, AST::File& curFile) const;

    template <typename T>
    Ptr<T> GetGenericDecl(Ptr<T> decl) const
    {
        if (decl->genericDecl) {
            return StaticCast<T>(decl->genericDecl);
        }

        return decl;
    }

    friend class TestManager;
    friend class MockManager;
    friend class MockSupportManager;
};
}

#endif // CANGJIE_SEMA_MOCK_UTILS_H
