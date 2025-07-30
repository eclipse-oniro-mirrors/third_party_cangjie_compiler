// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file handle extend interface which has default implement.
 */
#include "TypeCheckUtil.h"
#include "TypeCheckerImpl.h"

#include "cangjie/AST/Node.h"
#include "cangjie/Sema/TypeManager.h"

using namespace Cangjie;
using namespace TypeCheckUtil;
using namespace AST;

bool TypeChecker::TypeCheckerImpl::IsImplementation(
    Ty& baseTy, InterfaceTy& iTy, const Decl& interfaceMember, const Decl& childMember)
{
    if (interfaceMember.identifier != childMember.identifier) {
        return false;
    }
    CJC_ASSERT(childMember.outerDecl && interfaceMember.outerDecl);
    auto iFuncTy = interfaceMember.ty;
    // 1. Try to substitute generic types for generic function.
    if (interfaceMember.TestAttr(Attribute::GENERIC) && childMember.TestAttr(Attribute::GENERIC)) {
        TypeSubst typeMapping = typeManager.GenerateGenericMappingFromGeneric(interfaceMember, childMember);
        iFuncTy = typeManager.GetInstantiatedTy(iFuncTy, typeMapping);
    }
    // 1.5 Substitute interface member's type to baseTy inherited iTy.
    CJC_NULLPTR_CHECK(interfaceMember.outerDecl->ty);
    auto typeMappings = promotion.GetPromoteTypeMapping(iTy, *interfaceMember.outerDecl->ty);
    // For 'interface I<T> { func foo(a: T) {} }; class C <: I<A> & I<B>',
    // function 'foo' may have multiple instantiation type.
    auto interfaceMemberTys = typeManager.GetInstantiatedTys(iFuncTy, typeMappings);

    // 2. Substitute implemented member's type to baseTy.
    auto structTy = childMember.outerDecl->ty;
    CJC_NULLPTR_CHECK(structTy);
    typeMappings = promotion.GetPromoteTypeMapping(baseTy, *structTy);
    auto implementedMemberTy = typeManager.GetBestInstantiatedTy(childMember.ty, typeMappings);

    // 3. compare two member's signature. structure declaration mapping relation if signatures are same.
    // when member have multiple instantiated types, only need one of them passed.
    // Actual type will be decided in rearrange stage.
    return std::any_of(interfaceMemberTys.begin(), interfaceMemberTys.end(), [this, implementedMemberTy](Ptr<Ty> ty) {
        if (implementedMemberTy->IsFunc() && ty->IsFunc()) {
            return typeManager.IsFuncParameterTypesIdentical(
                *RawStaticCast<FuncTy*>(implementedMemberTy), *RawStaticCast<FuncTy*>(ty));
        }
        return implementedMemberTy == ty; // Must be prop decl's ty.
    });
}

bool TypeChecker::TypeCheckerImpl::HasOverrideDefaultImplement(
    const InheritableDecl& decl, const Decl& defaultImplement, InterfaceTy& superTy)
{
    auto hasImplementation = [&superTy, &decl, &defaultImplement, this](auto id) -> bool {
        for (const auto& member : id->GetMemberDecls()) {
            if (IsImplementation(*decl.ty, superTy, defaultImplement, *member)) {
                return true;
            }
        }
        return false;
    };
    auto id = Ty::GetDeclPtrOfTy<InheritableDecl>(decl.ty);
    if (id != nullptr && hasImplementation(id)) {
        return true;
    }
    auto sd = id && id->astKind == ASTKind::CLASS_DECL ? StaticCast<ClassDecl>(id)->GetSuperClassDecl() : nullptr;
    // Check whether the super class has inherited given interface member.
    while (sd != nullptr) {
        if (hasImplementation(sd)) {
            return true;
        }
        for (const auto& extend : typeManager.GetAllExtendsByTy(*sd->ty)) {
            if (hasImplementation(extend)) {
                return true;
            }
        }
        sd = sd->GetSuperClassDecl();
    }
    // Only extend decl needs to finding implementation from other extend decls.
    if (decl.astKind != ASTKind::EXTEND_DECL) {
        return false;
    }
    for (const auto& extend : typeManager.GetAllExtendsByTy(*decl.ty)) {
        if (hasImplementation(extend)) {
            return true;
        }
    }
    return false;
}

namespace {
using Orig2CopyMap = std::unordered_map<Ptr<Decl>, std::unordered_set<Ptr<Decl>>>;

void SetOuterDecl(Decl& decl, InheritableDecl& inheritableDecl)
{
    decl.DisableAttr(Attribute::IN_CLASSLIKE);
    decl.outerDecl = &inheritableDecl;
    if (inheritableDecl.IsClassLikeDecl()) {
        decl.EnableAttr(Attribute::IN_CLASSLIKE);
    } else if (inheritableDecl.astKind == ASTKind::STRUCT_DECL) {
        decl.EnableAttr(Attribute::IN_STRUCT);
    } else if (inheritableDecl.astKind == ASTKind::ENUM_DECL) {
        decl.EnableAttr(Attribute::IN_ENUM);
    } else if (inheritableDecl.astKind == ASTKind::EXTEND_DECL) {
        decl.EnableAttr(Attribute::IN_EXTEND);
    }
}

void SetOuterAndParentDecl(FuncDecl& funcDecl, InheritableDecl& inheritableDecl)
{
    SetOuterDecl(funcDecl, inheritableDecl);
    funcDecl.funcBody->parentClassLike = nullptr;
    if (inheritableDecl.ty->IsClassLike()) {
        funcDecl.funcBody->parentClassLike = RawStaticCast<ClassLikeDecl*>(Ty::GetDeclPtrOfTy(inheritableDecl.ty));
    } else if (inheritableDecl.ty->IsStruct()) {
        funcDecl.funcBody->parentStruct = RawStaticCast<StructDecl*>(Ty::GetDeclPtrOfTy(inheritableDecl.ty));
    } else if (inheritableDecl.ty->IsEnum()) {
        funcDecl.funcBody->parentEnum = RawStaticCast<EnumDecl*>(Ty::GetDeclPtrOfTy(inheritableDecl.ty));
    }
    if (!inheritableDecl.IsClassLikeDecl()) {
        // When sub type is not classlike decl. The 'open' should be removed.
        funcDecl.DisableAttr(Attribute::OPEN);
    }
}

void SortExtendByInherit(TypeManager& tyMgr, std::vector<Ptr<ExtendDecl>>& extends)
{
    auto compare = [&tyMgr](const Ptr<ExtendDecl> r, const Ptr<ExtendDecl> l) {
        auto ret = tyMgr.IsExtendInheritRelation(*r, *l);
        return ret.first ? ret.second : CompNodeByPos(r, l);
    };
    std::stable_sort(extends.begin(), extends.end(), compare);
}
} // namespace

OwnedPtr<Decl> TypeChecker::TypeCheckerImpl::GetCloneDecl(
    Decl& decl, InheritableDecl& inheritableDecl, InterfaceTy& superTy)
{
    auto addDecl = ASTCloner::Clone(Ptr(&decl), [&inheritableDecl](const Node& src, Node& cloned) {
        // Disable 'IMPORTED' attribute for correct exporting serialization.
        cloned.DisableAttr(Attribute::IMPORTED, Attribute::SRC_IMPORTED);
        // Need to update all decl's package name to copied package's name. For serialization's correct FullID.
        if (auto d = DynamicCast<Decl*>(&cloned); d) {
            d->moduleName = inheritableDecl.moduleName;
            d->fullPackageName = inheritableDecl.fullPackageName;
            d->toBeCompiled = inheritableDecl.toBeCompiled;
            // Clone hash for used as imported during incremental compilation.
            d->hash = StaticCast<Decl>(src).hash;
            if (!inheritableDecl.ty->IsStruct()) {
                d->DisableAttr(Attribute::MUT); // Non-struct type should not inherit `mut` attribute.
            }
        }
    });
    TypeSubst typeMapping;
    CJC_NULLPTR_CHECK(superTy.declPtr->ty);
    auto typeMappings = promotion.GetPromoteTypeMapping(superTy, *superTy.declPtr->ty);
    for (auto& it : std::as_const(typeMappings)) {
        typeMapping[it.first] = *it.second.begin();
    }
    auto preVisit = [&typeMapping, &inheritableDecl, this](Ptr<Node> node) -> VisitAction {
        node->ty = typeManager.GetInstantiatedTy(node->ty, typeMapping);
        if (auto ref = DynamicCast<NameReferenceExpr*>(node); ref) {
            for (auto& instTy : ref->instTys) {
                instTy = typeManager.GetInstantiatedTy(instTy, typeMapping);
            }
            ref->matchedParentTy = typeManager.GetInstantiatedTy(ref->matchedParentTy, typeMapping);
            // We need to update the type of 'this' to current inheritableDecl's type.
            if (auto re = DynamicCast<RefExpr*>(node); re && re->isThis) {
                re->ty = inheritableDecl.ty;
                re->ref.target = &inheritableDecl;
            }
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker(addDecl.get(), preVisit);
    walker.Walk();

    return addDecl;
}

void TypeChecker::TypeCheckerImpl::CloneAndInsert(
    Orig2CopyMap& originFuncToCopyFuncsMap, Decl& decl, InheritableDecl& inheritableDecl, InterfaceTy& superTy)
{
    // When the interface inherits the interface, only the static function needs to be copied. Non-static members cannot
    // be invoked by interface. Therefore, no copy is required.
    if (inheritableDecl.ty->IsInterface() && !decl.TestAttr(Attribute::STATIC)) {
        return;
    }
    if (decl.astKind == ASTKind::FUNC_DECL) {
        auto fd = RawStaticCast<FuncDecl*>(&decl);
        if (!fd->funcBody->body || HasOverrideDefaultImplement(inheritableDecl, *fd, superTy)) {
            return;
        }
        auto addFunc = GetCloneDecl(*fd, inheritableDecl, superTy);
        SetOuterAndParentDecl(*RawStaticCast<FuncDecl*>(addFunc.get()), inheritableDecl);
        originFuncToCopyFuncsMap[fd].insert(addFunc.get());
        inheritableDecl.GetMemberDecls().emplace_back(std::move(addFunc));
    } else if (decl.astKind == ASTKind::PROP_DECL) {
        auto pd = RawStaticCast<PropDecl*>(&decl);
        if (pd->TestAttr(Attribute::ABSTRACT) || HasOverrideDefaultImplement(inheritableDecl, *pd, superTy)) {
            return;
        }
        auto addProp = GetCloneDecl(*pd, inheritableDecl, superTy);
        auto addPropPtr = RawStaticCast<PropDecl*>(addProp.get());
        SetOuterDecl(*addPropPtr, inheritableDecl);
        for (size_t i = 0; i < addPropPtr->setters.size(); ++i) {
            SetOuterAndParentDecl(*addPropPtr->setters[i], inheritableDecl);
            originFuncToCopyFuncsMap[pd->setters[i].get()].insert(addPropPtr->setters[i].get());
        }
        for (size_t i = 0; i < addPropPtr->getters.size(); ++i) {
            SetOuterAndParentDecl(*addPropPtr->getters[i], inheritableDecl);
            originFuncToCopyFuncsMap[pd->getters[i].get()].insert(addPropPtr->getters[i].get());
        }
        if (!inheritableDecl.IsClassLikeDecl()) {
            // When sub type is not classlike decl. The 'open' should be removed.
            addProp->DisableAttr(Attribute::OPEN);
        }
        inheritableDecl.GetMemberDecls().emplace_back(std::move(addProp));
    }
}

namespace {
std::vector<Ptr<InheritableDecl>> SortByInheritanceChain(std::set<Ptr<InheritableDecl>>& otherDecls)
{
    std::vector<Ptr<InheritableDecl>> checked;
    while (!otherDecls.empty()) {
        auto preSize = otherDecls.size();
        // Each time, traverse to find the declarations at the top of the inheritance chain,
        // and remove them from origin set.
        // If the following conditions are met, it is considered as a top-level:
        // 1. Declaration is Enum/Struct type.
        // 2. Declaration haven't parent class type.
        // 3. Declaration's super class type is imported.
        // 4. Declaration's super class type is in checked.
        for (auto it = otherDecls.begin(); it != otherDecls.end();) {
            auto cd = DynamicCast<ClassDecl*>(*it);
            if (cd == nullptr) {
                (void)checked.emplace_back(*it);
                it = otherDecls.erase(it);
                continue;
            }
            auto superDecl = cd->GetSuperClassDecl();
            if (!superDecl || superDecl->TestAttr(Attribute::IMPORTED) ||
                std::find(checked.begin(), checked.end(), superDecl) != checked.end()) {
                (void)checked.emplace_back(*it);
                it = otherDecls.erase(it);
            } else {
                ++it;
            }
        }
        // If the top decl in inheritance chain cannot be found, they are emplace into the checked queue in default
        // order.
        if (otherDecls.size() == preSize) {
            (void)std::for_each(
                otherDecls.begin(), otherDecls.end(), [&checked](auto v) { (void)checked.emplace_back(v); });
            otherDecls.clear();
        }
    }
    return checked;
}
} // namespace

Orig2CopyMap TypeChecker::TypeCheckerImpl::CopyDefaultImplement(const Package& pkg)
{
    Orig2CopyMap originFuncToCopyFuncsMap;
    std::vector<Ptr<InterfaceDecl>> interfaceDecls;
    std::set<Ptr<InheritableDecl>> otherDecls;
    std::vector<Ptr<ExtendDecl>> extendDecls;
    // Processing for class/struct/enum/extend decls.
    auto collectDecls = [&interfaceDecls, &otherDecls, &extendDecls](auto& decl) {
        if (!decl->IsNominalDecl()) {
            return;
        }
        if (decl->astKind == ASTKind::INTERFACE_DECL) {
            interfaceDecls.emplace_back(RawStaticCast<InterfaceDecl*>(decl.get()));
        } else if (decl->astKind == ASTKind::EXTEND_DECL) {
            extendDecls.emplace_back(RawStaticCast<ExtendDecl*>(decl.get()));
        } else {
            otherDecls.emplace(RawStaticCast<InheritableDecl*>(decl.get()));
        }
    };
    for (auto& file : pkg.files) {
        for (auto& decl : file->decls) {
            collectDecls(decl);
        }
    }
    // Beacause of the inheritance relation, the individual declarations must be processed in the following order:
    // 1. All InterfaceDecls,
    // 2. ClassDecl/EnumDecl/StructDecl A and A's ExtendDecl,
    // 3. A's sub type declaration B and it's ExtendDecl.
    // 4. Multiple extensions of the same type are sorted in the order of inheritance of the implementing interfaces.
    std::vector<Ptr<InheritableDecl>> inheritableDecls(interfaceDecls.begin(), interfaceDecls.end());
    // Sort ClassDecl/EnumDecl/StructDecl by their inheritance relationship.
    for (auto decl : SortByInheritanceChain(otherDecls)) {
        inheritableDecls.emplace_back(decl);
        auto extends = typeManager.GetDeclExtends(*decl);
        Utils::EraseIf(extends, [&extendDecls](auto ed) { return !Utils::In(ed, extendDecls); });
        auto extendsVec = Utils::SetToVec<Ptr<ExtendDecl>>(extends);
        SortExtendByInherit(typeManager, extendsVec);
        inheritableDecls.insert(inheritableDecls.end(), extendsVec.begin(), extendsVec.end());
        Utils::EraseIf(extendDecls, [&extends](auto ed) { return Utils::In(ed, extends); });
    }
    inheritableDecls.insert(inheritableDecls.end(), extendDecls.begin(), extendDecls.end());
    for (auto& inheritableDecl : inheritableDecls) {
        // Notice: need BFS search to ensure nearer implementation will be copied.
        for (auto& superTy : typeManager.GetAllSuperInterfaceTysBFS(*inheritableDecl)) {
            for (auto& d : superTy->declPtr->body->decls) {
                CloneAndInsert(originFuncToCopyFuncsMap, *d, *inheritableDecl, *superTy);
            }
        }
    }
    return originFuncToCopyFuncsMap;
}

namespace {
Ptr<Ty> GetExpectedTy(TypeManager& tyMgr, Expr& expr)
{
    // Substituting type of expression's referenced target to real type.
    auto target = expr.GetTarget();
    auto ref = DynamicCast<NameReferenceExpr*>(&expr);
    bool invalid = !ref || !ref->matchedParentTy || !target || !target->outerDecl || !target->outerDecl->ty;
    if (invalid) {
        return nullptr;
    }
    auto parentTy = target->outerDecl->ty;
    if (ref->matchedParentTy->typeArgs.size() != parentTy->typeArgs.size()) {
        return target->ty;
    }
    TypeSubst typeMapping;
    for (size_t i = 0; i < ref->matchedParentTy->typeArgs.size(); ++i) {
        typeMapping[StaticCast<GenericsTy*>(parentTy->typeArgs[i])] = ref->matchedParentTy->typeArgs[i];
    }
    auto expectedTy = tyMgr.GetInstantiatedTy(target->ty, typeMapping);
    return expectedTy;
}

void RearrangeDefaultImplRefExpr(
    TypeManager& tyMgr, Ptr<const Decl> currCompositeDecl, RefExpr& re, const Orig2CopyMap& originFuncToCopyFuncsMap)
{
    if (!re.ref.target) {
        return;
    }
    // Update refExpr's target if it was genericParamDecl, like 'T' of 'T.xxx'.
    if (re.ref.target->astKind == ASTKind::GENERIC_PARAM_DECL && re.ty && !re.ty->IsGeneric()) {
        re.ref.target = Ty::GetDeclPtrOfTy(re.ty);
        if (Is<InheritableDecl*>(re.ref.target)) {
            re.instTys = re.ty->typeArgs;
        }
        return;
    }
    auto found = originFuncToCopyFuncsMap.find(re.ref.target);
    if (found == originFuncToCopyFuncsMap.end() || !currCompositeDecl) {
        bool needClear = re.ref.target->outerDecl == currCompositeDecl && re.ref.target->TestAttr(Attribute::DEFAULT);
        // Clear 'matchedParentTy' if default implementation is already in current composite decl.
        if (needClear) {
            re.matchedParentTy = nullptr;
        }
        return;
    }
    auto expectedTy = GetExpectedTy(tyMgr, re);
    if (!expectedTy) {
        return;
    }
    bool updated = false;
    for (auto& it : found->second) {
        CJC_ASSERT(it->outerDecl && it->outerDecl->ty);
        // 1. Check the current outerDecl's type is the subtype of function's outerDecl's ytpe equality for nominal ty;
        // 2. If outerDecl's type is not nominal, the current outerDecl should be extend of builtIn decls
        // so check the type kinds equal.
        bool matchedDecl;
        if (it->outerDecl->ty->IsNominal()) {
            // For unifying the type arguments of two decl types to checking inheritance relation.
            matchedDecl = tyMgr.IsSubtype(currCompositeDecl->ty, it->outerDecl->ty);
        } else {
            matchedDecl = currCompositeDecl->ty->kind == it->outerDecl->ty->kind;
        }
        // When outerDecl is extend, use Ty::GetDeclPtrOfTy to get the extended type.
        if (matchedDecl && it->ty == expectedTy) {
            if (updated) {
                InternalError("Semantic check should ensure only one candidate will be chosen.");
            }
            re.ref.target = it;
            re.matchedParentTy = nullptr;
            updated = true;
        }
    }
}

void RearrangeDefaultImplMemberAccess(
    TypeManager& tyMgr, MemberAccess& ma, const Orig2CopyMap& originFuncToCopyFuncsMap)
{
    if (!ma.target) {
        return;
    }
    auto found = originFuncToCopyFuncsMap.find(ma.target);
    if (found == originFuncToCopyFuncsMap.end()) {
        return;
    }
    auto expectedTy = GetExpectedTy(tyMgr, ma);
    if (!expectedTy) {
        return;
    }
    // Generate typeMapping from member base expression for substituting target's type.
    MultiTypeSubst mts;
    tyMgr.GenerateGenericMapping(mts, *ma.baseExpr->ty);
    auto mapping = MultiTypeSubstToTypeSubst(mts);
    bool updated = false;
    auto baseDecl = Ty::GetDeclPtrOfTy(ma.baseExpr->ty);
    for (auto& it : found->second) {
        // If the baseDecl is exist, the memberAccess can access current function
        // when baseDecl's type is subtype of function's outerDecl's type.
        // If baseDecl is not exist, the current outerDecl should be extend of builtIn decls
        // so check the type kinds equal.
        bool matchedDecl;
        if (baseDecl) {
            // Generate typeMapping from member base expression for substituting target's type.
            auto m = GenerateTypeMapping(*it->outerDecl, baseDecl->ty->typeArgs);
            // For unifying the type arguments of two decl types to checking inheritance relation.
            matchedDecl = it->TestAttr(Attribute::STATIC)
                ? baseDecl->ty == tyMgr.GetInstantiatedTy(it->outerDecl->ty, m)
                : tyMgr.IsSubtype(baseDecl->ty, tyMgr.GetInstantiatedTy(it->outerDecl->ty, m));
        } else {
            matchedDecl = ma.baseExpr->ty->kind == it->outerDecl->ty->kind;
        }
        if (matchedDecl && tyMgr.GetInstantiatedTy(it->ty, mapping) == expectedTy) {
            if (updated) {
                InternalError("Semantic check should ensure only one candidate will be chosen.");
            }
            ma.target = it;
            ma.matchedParentTy = nullptr;
            updated = true;
        }
    }
}

void RearrangeDefaultImplCallExpr(CallExpr& ce)
{
    // Since using post visit, the baseFunc of callExpr must have been rearranged.
    if (!ce.resolvedFunction || !ce.baseFunc || !ce.baseFunc->IsReferenceExpr()) {
        return;
    }
    ce.resolvedFunction = RawStaticCast<FuncDecl*>(ce.baseFunc->GetTarget());
}

void RearrangeDefaultCall(const Package& pkg, const Orig2CopyMap& originFuncToCopyFuncsMap, TypeManager& tyMgr)
{
    Ptr<Decl> currCompositeDecl = nullptr;
    auto preVist = [&currCompositeDecl](Ptr<Node> node) -> VisitAction {
        switch (node->astKind) {
            case ASTKind::EXTEND_DECL:
            case ASTKind::ENUM_DECL:
            case ASTKind::STRUCT_DECL:
            case ASTKind::CLASS_DECL:
            case ASTKind::INTERFACE_DECL:
                currCompositeDecl = RawStaticCast<Decl*>(node);
                break;
            case ASTKind::FUNC_DECL: {
                // Ignore non-static member in interface, only static function can be rearranged inside interface.
                bool ignore = currCompositeDecl && currCompositeDecl->astKind == ASTKind::INTERFACE_DECL &&
                    !node->TestAttr(Attribute::STATIC);
                return ignore ? VisitAction::SKIP_CHILDREN : VisitAction::WALK_CHILDREN;
            }
            default:
                break;
        }
        return VisitAction::WALK_CHILDREN;
    };
    auto postVisit = [&originFuncToCopyFuncsMap, &currCompositeDecl, &tyMgr](Ptr<Node> node) -> VisitAction {
        switch (node->astKind) {
            case ASTKind::REF_EXPR:
                RearrangeDefaultImplRefExpr(
                    tyMgr, currCompositeDecl, *RawStaticCast<RefExpr*>(node), originFuncToCopyFuncsMap);
                break;
            case ASTKind::MEMBER_ACCESS:
                RearrangeDefaultImplMemberAccess(tyMgr, *RawStaticCast<MemberAccess*>(node), originFuncToCopyFuncsMap);
                break;
            case ASTKind::CALL_EXPR:
                RearrangeDefaultImplCallExpr(*RawStaticCast<CallExpr*>(node));
                break;
            default:
                break;
        }
        return VisitAction::WALK_CHILDREN;
    };
    for (auto& file : pkg.files) {
        Walker walker(file.get(), preVist, postVisit);
        walker.Walk();
    }
}
} // namespace

void TypeChecker::TypeCheckerImpl::HandleDefaultImplement(const Package& pkg)
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    if (ci->invocation.globalOptions.disableInstantiation) {
        return;
    }
#endif
    // Only done for source package.
    auto originFuncToCopyFuncsMap = CopyDefaultImplement(pkg);
    RearrangeDefaultCall(pkg, originFuncToCopyFuncsMap, typeManager);
}
