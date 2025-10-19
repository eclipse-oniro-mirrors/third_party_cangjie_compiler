// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.
/**
 * @file
 *
 * This file implements the header file Promotion.h
 */
#include "Promotion.h"

#include "cangjie/AST/Types.h"
#include "cangjie/AST/ASTCasting.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie;
using namespace AST;

// Given C<X> <: D<X>, Promote(C<Bool>, D<X>) will establish a substitution [X |-> [Bool]].
// Given C<X> <: D<X> & C<X> <: D<Int64>, Promote(C<Bool>, D<X>) will establish a substitution [X |-> [Bool, Int64]].
MultiTypeSubst Promotion::GetPromoteTypeMapping(Ty& from, Ty& target)
{
    if (!Ty::IsTyCorrect(&from) || !Ty::IsTyCorrect(&target)) {
        return {};
    }
    auto prRes = Promote(from, target);
    if (prRes.empty()) {
        return {};
    }

    MultiTypeSubst mts;

    Utils::EraseIf(prRes, [&target](auto& e) { return e->typeArgs.size() != target.typeArgs.size(); });
    std::for_each(prRes.begin(), prRes.end(), [&target, &mts](auto prResI) {
        for (size_t i = 0; i < target.typeArgs.size(); i++) {
            if (auto targetGenParam = DynamicCast<GenericsTy*>(target.typeArgs[i])) {
                mts[targetGenParam].emplace(prResI->typeArgs[i]);
            }
        }
    });

    return mts;
}

MultiTypeSubst Promotion::GetDowngradeTypeMapping(Ty& target, Ty& upfrom)
{
    if (!Ty::IsTyCorrect(&target) || !Ty::IsTyCorrect(&upfrom)) {
        return {};
    }
    auto prRes = Promote(target, upfrom);
    if (prRes.empty()) {
        return {};
    }

    MultiTypeSubst mts;

    Utils::EraseIf(prRes, [&upfrom](auto& e) { return e->typeArgs.size() != upfrom.typeArgs.size(); });
    std::for_each(prRes.begin(), prRes.end(), [&upfrom, &mts](auto prResI) {
        for (size_t i = 0; i < prResI->typeArgs.size(); i++) {
            if (auto tv = DynamicCast<GenericsTy*>(prResI->typeArgs[i])) {
                mts[tv].emplace(upfrom.typeArgs[i]);
            }
        }
    });

    return mts;
}

// Given C<X> <: D<X>, Promote(C<Bool>, D<_>) will give a promoted singleton set {D<Bool>}.
// Given C<X> <: D<X> & C<X> <: D<Int64>, Promote(C<Bool>, D<_>) will give a promoted set {D<Bool>, D<Int64>}
std::set<Ptr<Ty>> Promotion::Promote(Ty& from, Ty& target)
{
    if (!Ty::IsTyCorrect(&from) || !Ty::IsTyCorrect(&target)) {
        return {};
    }
    // Promoting to 'target' type when any of the following conditions is met;
    // 1. 'from' is type of 'Nothing';
    // 2. 'target' is type of 'Any';
    // 3. 'from' is a type which meets CType constraints and 'target' is 'CType';
    // 4. 'from' is a type which meets JType constraints and 'target' is 'JType'.
    bool useTarget = from.IsNothing() || target.IsAny() || (target.IsCType() && Ty::IsMetCType(from));
    if (useTarget) {
        return {&target};
    }
    if (from.IsPrimitive() && target.IsPrimitive()) {
        return PromoteHandleIdealTys(from, target);
    }
    if (from.IsFunc() && target.IsFunc()) {
        return PromoteHandleFunc(from, target);
    }
    if (from.IsTuple() && target.IsTuple()) {
        return PromoteHandleTuple(from, target);
    }
    if (auto res = PromoteHandleTyVar(from, target); !res.empty()) {
        return res;
    }
    return PromoteHandleNominal(from, target);
}

std::set<Ptr<AST::Ty>> Promotion::Downgrade(AST::Ty& target, AST::Ty& upfrom)
{
    auto dMaps = GetDowngradeTypeMapping(target, upfrom);
    if (dMaps.size() < target.typeArgs.size()) {
        return {};
    } else {
        return tyMgr.GetInstantiatedTys(&target, dMaps);
    }
}

std::set<Ptr<Ty>> Promotion::PromoteHandleTyVar(Ty& from, Ty& target)
{
    if (!from.IsGeneric()) {
        return {};
    }
    auto genericTy = RawStaticCast<GenericsTy*>(&from);
    for (auto& ty : genericTy->upperBounds) {
        if (auto res = Promote(*ty, target); !res.empty()) {
            return res;
        }
    }
    return {};
}

std::set<Ptr<Ty>> Promotion::PromoteHandleIdealTys(Ty& from, Ty& target) const
{
    // Caller guarantees the 'from' and 'target' are primitive type.
    if (from.kind == target.kind) {
        return {&from};
    }
    // Very ad hoc fix. Should be improved in the future.
    if (from.kind == TypeKind::TYPE_IDEAL_INT && target.IsInteger()) {
        return {&target};
    } else if (from.kind == TypeKind::TYPE_IDEAL_FLOAT && target.IsFloating()) {
        return {&target};
    } else {
        return {};
    }
}

std::set<Ptr<Ty>> Promotion::PromoteHandleFunc(Ty& from, Ty& target)
{
    if (tyMgr.IsTyEqual(&from, &target)) {
        return {&from};
    } else {
        return {};
    }
}

std::set<Ptr<Ty>> Promotion::PromoteHandleTuple(Ty& from, Ty& target)
{
    if (tyMgr.IsTyEqual(&from, &target)) {
        return {&from};
    } else {
        return {};
    }
}

std::set<Ptr<Ty>> Promotion::PromoteHandleNominal(Ty& from, const Ty& target)
{
    if (Ty::GetDeclPtrOfTy<InheritableDecl>(&from) == Ty::GetDeclPtrOfTy<InheritableDecl>(&target)) {
        return {&from};
    }

    TypeSubst typeMapping;
    auto emplaceElem = [&from, &typeMapping](const std::vector<Ptr<Ty>>& tyVars) {
        if (tyVars.size() != from.typeArgs.size()) {
            return;
        }
        for (size_t i = 0; i < tyVars.size(); i++) {
            typeMapping.emplace(StaticCast<GenericsTy*>(tyVars[i]), from.typeArgs[i]);
        }
    };
    if (from.IsClass()) {
        auto classTy = RawStaticCast<ClassTy*>(&from);
        CJC_ASSERT(classTy->decl);
        if (classTy->decl->ty) {
            emplaceElem(classTy->declPtr->ty->typeArgs);
        }
    } else if (from.IsInterface()) {
        auto interfaceTy = RawStaticCast<InterfaceTy*>(&from);
        CJC_ASSERT(interfaceTy->decl);
        if (interfaceTy->decl->ty) {
            emplaceElem(interfaceTy->declPtr->ty->typeArgs);
        }
    } else if (from.IsStruct()) {
        auto structTy = RawStaticCast<StructTy*>(&from);
        CJC_ASSERT(structTy->decl);
        if (structTy->decl->ty) {
            emplaceElem(structTy->declPtr->ty->typeArgs);
        }
    } else if (from.IsEnum()) {
        auto enumTy = RawStaticCast<EnumTy*>(&from);
        CJC_ASSERT(enumTy->decl);
        if (enumTy->decl->ty) {
            emplaceElem(enumTy->declPtr->ty->typeArgs);
        }
    }

    std::set<Ptr<Ty>> res;
    auto supTys = tyMgr.GetAllSuperTys(from, typeMapping);
    for (auto ty : supTys) {
        auto cly = DynamicCast<ClassLikeTy*>(ty);
        if (cly && Ty::GetDeclPtrOfTy(cly) == Ty::GetDeclPtrOfTy(&target)) {
            res.insert(cly);
        }
    }
    return res;
}
