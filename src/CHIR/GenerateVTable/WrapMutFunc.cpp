// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/GenerateVTable/WrapMutFunc.h"

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

namespace {
void GetAllInstantiatedParentType2(ClassType& cur, ClassType& targetParent, CHIRBuilder& builder,
    std::vector<ClassType*>& parents, std::unordered_map<const GenericType*, Type*>& replaceTable,
    std::unordered_set<ClassType*>& visited, bool& stop)
{
    if (stop) {
        return;
    }
    if (std::find(visited.begin(), visited.end(), &cur) != visited.end()) {
        return;
    }
    if (&cur == &targetParent) {
        stop = true;
    }
    for (auto ex : cur.GetCustomTypeDef()->GetExtends()) {
        // maybe we can meet `extend<T> A<B<T>> {}`, and `curType` is A<Int32>, then ignore this def,
        // so not need to check `res`
        auto [res, extendTable] = ex->GetExtendedType()->CalculateGenericTyMapping(cur);
        for (auto interface : ex->GetImplementedInterfaceTys()) {
            GetAllInstantiatedParentType2(*interface, targetParent, builder, parents, extendTable, visited, stop);
            if (!stop) {
                CJC_ASSERT(parents.back() == interface);
                parents.pop_back();
            }
        }
    }
    for (auto interface : cur.GetImplementedInterfaceTys(&builder)) {
        GetAllInstantiatedParentType2(*interface, targetParent, builder, parents, replaceTable, visited, stop);
        if (!stop) {
            CJC_ASSERT(parents.back() == interface);
            parents.pop_back();
        }
    }
    if (cur.GetSuperClassTy(&builder) != nullptr) {
        auto superClass = cur.GetSuperClassTy(&builder);
        GetAllInstantiatedParentType2(*superClass, targetParent, builder, parents, replaceTable, visited, stop);
        if (!stop) {
            CJC_ASSERT(parents.back() == superClass);
            parents.pop_back();
        }
    }
    visited.emplace(&cur);
    parents.emplace_back(Cangjie::StaticCast<ClassType*>(ReplaceRawGenericArgType(cur, replaceTable, builder)));
}

std::vector<ClassType*> GetTargetInheritanceList(CustomTypeDef& curDef, ClassType& targetParent, CHIRBuilder& builder)
{
    std::vector<ClassType*> inheritanceList;
    std::unordered_set<ClassType*> visited;
    std::unordered_map<const GenericType*, Type*> emptyTable;
    bool stop = false;
    for (auto interface : curDef.GetImplementedInterfaceTys()) {
        GetAllInstantiatedParentType2(*interface, targetParent, builder, inheritanceList, emptyTable, visited, stop);
        if (!stop) {
            CJC_ASSERT(inheritanceList.back() == interface);
            inheritanceList.pop_back();
        }
    }
    if (curDef.IsClassLike()) {
        auto superClass = StaticCast<ClassDef*>(&curDef)->GetSuperClassTy();
        if (superClass != nullptr) {
            GetAllInstantiatedParentType2(
                *superClass, targetParent, builder, inheritanceList, emptyTable, visited, stop);
            if (!stop) {
                CJC_ASSERT(inheritanceList.back() == superClass);
                inheritanceList.pop_back();
            }
        }
    }
    CJC_ASSERT(stop);
    return inheritanceList;
}

std::unordered_map<const GenericType*, Type*> CollectReplaceTableFromAllParents(
    CustomTypeDef& curDef, ClassType& srcClassTy, CHIRBuilder& builder)
{
    std::unordered_map<const GenericType*, Type*> replaceTable;

    auto parentTypes = GetTargetInheritanceList(curDef, srcClassTy, builder);
    for (auto parentType : parentTypes) {
        auto instTypeArgs = parentType->GetTypeArgs();
        auto genericTypeArgs = parentType->GetCustomTypeDef()->GetGenericTypeParams();
        for (size_t i = 0; i < genericTypeArgs.size(); ++i) {
            replaceTable.emplace(genericTypeArgs[i], instTypeArgs[i]);
        }
    }
    return replaceTable;
}

/**
 * @brief Try to get instantiated sub type.
 *
 * @param genericSubType generic sub type.
 * @param instParentType instantiated parent type
 * @param builder CHIR builder
 * @return return genericSubType when
 *  1. `genericSubType` is not generic related
 *  2. `genericSubType` equals to `instParentType`
 *  3. `genericSubType` and `instParentType` don't have parent-child relationship
 *  return instantiated sub type when
 *  1. `genericSubType` and `instParentType` are same CustomTypeDef,
 *     but one is generic related, the other is instantiated type
 *  2. `genericSubType` and `instParentType` have parent-child relationship,
 *     and `genericSubType` is generic related sub type, `instParentType` is instantiated parent type
 */
Type* GetInstSubType(Type& genericSubType, const ClassType& instParentType, CHIRBuilder& builder)
{
    if (!genericSubType.IsGenericRelated()) {
        return &genericSubType;
    }
    if (&genericSubType == &instParentType) {
        return &genericSubType;
    }
    auto [res, replaceTable] = genericSubType.CalculateGenericTyMapping(instParentType);
    if (!res) {
        for (auto p : genericSubType.GetSuperTypesRecusively(builder)) {
            std::tie(res, replaceTable) = p->CalculateGenericTyMapping(instParentType);
            if (res) {
                break;
            }
        }
    }
    CJC_ASSERT(res);
    return ReplaceRawGenericArgType(genericSubType, replaceTable, builder);
}
} // namespace

void WrapMutFunc::CreateMutFuncWrapper(FuncBase* rawFunc, CustomTypeDef& curDef, ClassType& srcClassTy)
{
    // create the wrapper func
    auto replaceTable = CollectReplaceTableFromAllParents(curDef, srcClassTy, builder);

    auto instFuncTy = StaticCast<FuncType*>(ReplaceRawGenericArgType(*rawFunc->GetFuncType(), replaceTable, builder));
    auto wrapperParamsTy = instFuncTy->GetParamTypes();
    auto parentDefType = curDef.IsExtend() ? StaticCast<ExtendDef>(curDef).GetExtendedType() : curDef.GetType();
    wrapperParamsTy[0] = builder.GetType<RefType>(parentDefType);
    auto retTy = instFuncTy->GetReturnType();
    auto wrapperFuncTy = builder.GetType<FuncType>(wrapperParamsTy, retTy);

    auto funcIdentifier = CHIRMangling::GenerateVirtualFuncMangleName(rawFunc, curDef, &srcClassTy, false);
    auto pkgName = curDef.GetPackageName();

    bool isImported = curDef.TestAttr(Attribute::IMPORTED);
    FuncBase* funcBase = nullptr;
    if (isImported) {
        funcBase = builder.CreateImportedVarOrFunc<ImportedFunc>(wrapperFuncTy, funcIdentifier, "", "", pkgName);
    } else {
        funcBase = builder.CreateFunc(INVALID_LOCATION, wrapperFuncTy, funcIdentifier, "", "", pkgName);
    }
    wrapperFuncs.emplace(funcIdentifier, funcBase);
    CJC_NULLPTR_CHECK(funcBase);

    funcBase->Set<WrappedRawMethod>(rawFunc);
    funcBase->AppendAttributeInfo(rawFunc->GetAttributeInfo());
    funcBase->DisableAttr(Attribute::VIRTUAL);
    funcBase->EnableAttr(Attribute::NO_REFLECT_INFO);
    curDef.AddMethod(funcBase);

    if (isImported) {
        return;
    }

    auto func = DynamicCast<Func*>(funcBase);
    CJC_NULLPTR_CHECK(func);
    // create the func body
    BlockGroup* body = builder.CreateBlockGroup(*func);
    func->InitBody(*body);

    std::vector<Value*> args;
    for (auto paramTy : wrapperParamsTy) {
        args.emplace_back(builder.CreateParameter(paramTy, INVALID_LOCATION, *func));
    }

    auto entry = builder.CreateBlock(body);
    body->SetEntryBlock(entry);
    auto ret =
        Cangjie::CHIR::CreateAndAppendExpression<Allocate>(builder, builder.GetType<RefType>(retTy), retTy, entry);
    func->SetReturnValue(*ret->GetResult());

    auto rawFuncFirstArgType = rawFunc->GetFuncType()->GetParamTypes()[0]->StripAllRefs();
    auto firstArgType = GetInstSubType(*rawFuncFirstArgType, srcClassTy, builder);
    if (!firstArgType->IsValueType() || rawFunc->TestAttr(Attribute::MUT)) {
        firstArgType = builder.GetType<RefType>(firstArgType);
    }
    args[0] = Cangjie::CHIR::TypeCastOrBoxIfNeeded(*args[0], *firstArgType, builder, *entry, INVALID_LOCATION);

    auto apply = Cangjie::CHIR::CreateAndAppendExpression<Apply>(builder, retTy, rawFunc, FuncCallContext{
        .args = args,
        .thisType = curDef.GetType()}, entry);
    Cangjie::CHIR::CreateAndAppendExpression<Store>(
        builder, builder.GetUnitTy(), apply->GetResult(), func->GetReturnValue(), entry);

    auto tempThis =
        Cangjie::CHIR::TypeCastOrBoxIfNeeded(*args[0], *wrapperParamsTy[0], builder, *entry, INVALID_LOCATION);
    auto load = Cangjie::CHIR::CreateAndAppendExpression<Load>(builder, parentDefType, tempThis, entry)->GetResult();
    auto structMemberTypes = StaticCast<StructType*>(parentDefType)->GetInstantiatedMemberTys(builder);

    for (size_t i = 0; i < structMemberTypes.size(); ++i) {
        auto path = std::vector<uint64_t>{i};
        auto field = Cangjie::CHIR::CreateAndAppendExpression<Field>(builder, structMemberTypes[i], load, path, entry)
            ->GetResult();
        Cangjie::CHIR::CreateAndAppendExpression<StoreElementRef>(
            builder, builder.GetUnitTy(), field, func->GetParam(0), path, entry);
    }

    entry->AppendExpression(builder.CreateTerminator<Exit>(entry));
}

void WrapMutFunc::Run(CustomTypeDef& customTypeDef)
{
    Type* structTy = nullptr;
    if (customTypeDef.GetCustomKind() == CustomDefKind::TYPE_EXTEND &&
        StaticCast<ExtendDef>(customTypeDef).GetExtendedType()->IsStruct()) {
        structTy = StaticCast<ExtendDef>(customTypeDef).GetExtendedType();
    } else if (customTypeDef.GetCustomKind() == CustomDefKind::TYPE_STRUCT &&
        StaticCast<StructDef>(customTypeDef).GetImplementedInterfacesNum() > 0) {
        structTy = customTypeDef.GetType();
    }
    if (!structTy) {
        return;
    }
    for (auto& [srcTy, infos] : customTypeDef.GetVTable()) {
        for (size_t i = 0; i < infos.size(); ++i) {
            if (!infos[i].instance) {
                continue;
            }
            CJC_NULLPTR_CHECK(infos[i].instance);
            auto rawFunc = infos[i].instance;
            while (auto base = rawFunc->Get<WrappedRawMethod>()) {
                rawFunc = base;
            }
            if (!rawFunc->TestAttr(Attribute::MUT) || rawFunc->GetParentCustomTypeDef() == &customTypeDef) {
                continue;
            }
            if (auto ex = DynamicCast<ExtendDef*>(&customTypeDef);
                ex && ex->GetExtendedCustomTypeDef() == rawFunc->GetParentCustomTypeDef()) {
                continue;
            }
            CreateMutFuncWrapper(rawFunc, customTypeDef, *StaticCast<ClassType*>(srcTy));
        }
    }
}

WrapMutFunc::WrapMutFunc(CHIRBuilder& b) : builder(b)
{
}

std::unordered_map<std::string, FuncBase*>&& WrapMutFunc::GetWrappers()
{
    return std::move(wrapperFuncs);
}
