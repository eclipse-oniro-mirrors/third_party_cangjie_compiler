// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the de-virtualization optimization.
 */

#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"

#include "cangjie/CHIR/CHIRContext.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Utils/Casting.h"

namespace Cangjie::CHIR {

void DevirtualizationInfo::CollectInfo()
{
    // Function that collect all global function which has more concrete type than
    // explicit return type in order to infer devirtualization more precise.
    for (auto func : package->GetGlobalFuncs()) {
        CollectReturnTypeMap(*func);
    }

    // Collecting inheritance relationships between type definitions.
    for (const auto customTypeDef : package->GetAllCustomTypeDef()) {
        if (customTypeDef->IsClassLike() && IsClosureConversionEnvClass(*StaticCast<const ClassDef*>(customTypeDef))) {
            continue;
        }

        Type* thisType = customTypeDef->GetType();
        if (customTypeDef->IsExtend()) {
            thisType = StaticCast<ExtendDef*>(customTypeDef)->GetExtendedType();
            if (auto customTy = DynamicCast<CustomType*>(thisType)) {
                thisType = customTy->GetCustomTypeDef()->GetType();
            }
        }

        defsMap[thisType].emplace_back(customTypeDef);
        for (auto parentTy : customTypeDef->GetSuperTypesInCurDef()) {
            auto parentDef = parentTy->GetClassDef();
            if (IsCoreObject(*parentDef)) {
                continue;
            }
            // need more accurate range which cannot be inherit.
            if (parentDef->TestAttr(Attribute::PRIVATE)) {
                subtypeMap[parentDef].emplace_back(InheritanceInfo{parentTy, thisType});
                continue;
            }
        }
    }
}

const DevirtualizationInfo::SubTypeMap& DevirtualizationInfo::GetSubtypeMap() const
{
    return subtypeMap;
}

const std::unordered_map<Func*, Type*>& DevirtualizationInfo::GetReturnTypeMap() const
{
    return realRetTyMap;
}

void DevirtualizationInfo::FreshRetMap()
{
    for (auto func : package->GetGlobalFuncs()) {
        auto retVal = func->GetReturnValue();
        if (!retVal) {
            continue;
        }
        auto rtTy = retVal->GetType();
        while (rtTy->IsRef()) {
            rtTy = StaticCast<RefType *>(rtTy)->GetBaseType();
        }
        // Do not process the return type that is not class type.
        if (!rtTy->IsClass()) {
            continue;
        }
        std::vector<Expression*> users = retVal->GetUsers();
        if (users.size() == 1 && users[0]->GetExprKind() == ExprKind::STORE) {
            auto val = StaticCast<Store*>(users[0])->GetValue();
            if (!val->IsLocalVar()) {
                continue;
            }
            auto expr = StaticCast<LocalVar *>(val)->GetExpr();
            auto srcTy = val->GetType();
            if (expr->GetExprKind() == ExprKind::TYPECAST) {
                auto cast = StaticCast<TypeCast *>(expr);
                srcTy = cast->GetSourceTy();
            }
            // Ensure that the actual return value type is not a reference.
            while (srcTy->IsRef()) {
                srcTy = StaticCast<RefType *>(srcTy)->GetBaseType();
            }
            realRetTyMap[func] = srcTy;
        }
    }
}

// Function that collect all global function which has more concrete type
// than explicit return type in order to infer devirtualization more precise.
void DevirtualizationInfo::CollectReturnTypeMap(Func& func)
{
    auto retVal = func.GetReturnValue();
    if (!retVal) {
        return;
    }
    auto rtTy = retVal->GetType();
    while (rtTy->IsRef()) {
        rtTy = StaticCast<RefType*>(rtTy)->GetBaseType();
    }
    // Do not process the return type that is not class type.
    if (!rtTy->IsClass()) {
        return;
    }
    std::vector<Expression*> users = retVal->GetUsers();
    if (users.size() == 1 && users[0]->GetExprKind() == ExprKind::STORE) {
        auto val = StaticCast<Store*>(users[0])->GetValue();
        if (!val->IsLocalVar()) {
            return;
        }
        auto expr = StaticCast<LocalVar*>(val)->GetExpr();
        auto srcTy = val->GetType();
        if (expr->GetExprKind() == ExprKind::TYPECAST) {
            auto cast = StaticCast<TypeCast*>(expr);
            srcTy = cast->GetSourceTy();
        }
        // Ensure that the actual return value type is not a reference.
        while (srcTy->IsRef()) {
            srcTy = StaticCast<RefType*>(srcTy)->GetBaseType();
        }
        realRetTyMap.emplace(&func, srcTy);
    }
}
} // namespace Cangjie::CHIR
