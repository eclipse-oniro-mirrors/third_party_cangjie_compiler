// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Transformation/Devirtualization.h"

#include "cangjie/CHIR/Transformation/DeadCodeElimination.h"
#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/Analysis/Engine.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/CHIR/Transformation/BlockGroupCopyHelper.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"
namespace Cangjie::CHIR {

Devirtualization::Devirtualization(
    TypeAnalysisWrapper* typeAnalysisWrapper, DevirtualizationInfo& devirtFuncInfo)
    : analysisWrapper(typeAnalysisWrapper), devirtFuncInfo(devirtFuncInfo)
{
}

void Devirtualization::RunOnFuncs(const std::vector<Func*>& funcs, CHIRBuilder& builder, bool isDebug)
{
    rewriteInfos.clear();
    for (auto func : funcs) {
        if (func->Get<SkipCheck>() == SkipKind::SKIP_CODEGEN) {
            continue;
        }
        RunOnFunc(func, builder);
    }
    RewriteToApply(builder, rewriteInfos, isDebug);
    InstantiateFuncIfPossible(builder, rewriteInfos);
}

void Devirtualization::RunOnFunc(const Func* func, CHIRBuilder& builder)
{
    auto result = analysisWrapper->CheckFuncResult(func);
    if (result == nullptr && frozenStates.count(func) != 0) {
        result = frozenStates.at(func).get();
    }
    CJC_ASSERT(result);

    const auto actionBeforeVisitExpr = [this, &builder](const TypeDomain& state, Expression* expr, size_t) {
        if (expr->GetExprKind() != ExprKind::INVOKE) {
            return;
        }
        auto invoke = StaticCast<Invoke*>(expr);
        auto object = invoke->GetObject();
        auto invokeAbsObject = state.CheckAbstractObjectRefBy(object);
        // Obtains the state information of the invoke operation object.
        auto resVal = state.CheckAbstractValue(invokeAbsObject);
        if (!resVal) {
            return;
        }
        std::vector<Type*> paramTys = invoke->GetInstantiatedParamTypes();
        // Grab the function from the classMap.
        auto [realCallee, thisType] = FindRealCallee(builder, resVal,
            {invoke->GetMethodName(), std::move(paramTys), invoke->GetInstantiatedTypeArgs()});
        if (!realCallee) {
            return;
        }
        rewriteInfos.emplace_back(RewriteInfo{invoke, realCallee, thisType, invoke->GetInstantiatedTypeArgs()});
    };

    const auto actionAfterVisitExpr = [](const TypeDomain&, Expression*, size_t) {};
    const auto actionOnTerminator = [](const TypeDomain&, Terminator*, std::optional<Block*>) {};
    result->VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);
}

namespace {
struct BuiltinOpInfo {
    BuiltinOpInfo(FuncInfo info, ExprKind exprKind, size_t operandsNum)
        : funcInfo(std::move(info)), targetExprKind(exprKind), operandsNum(operandsNum) {}

    FuncInfo funcInfo;
    ExprKind targetExprKind{ExprKind::INVALID};
    size_t operandsNum{0};
};

static const std::vector<BuiltinOpInfo> COMPARABLE_FUNC_LISTS = {
    {FuncInfo(">", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),    ExprKind::GT,       2U},
    {FuncInfo("<", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),    ExprKind::LT,       2U},
    {FuncInfo(">=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   ExprKind::GE,       2U},
    {FuncInfo("<=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   ExprKind::LE,       2U},
    {FuncInfo("==", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   ExprKind::EQUAL,    2U},
    {FuncInfo("!=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   ExprKind::NOTEQUAL, 2U},
    {FuncInfo("next", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"), ExprKind::APPLY,    2U}
};

Ptr<Apply> BuiltinOpCreateNewApply(CHIRBuilder& builder, const Invoke& oriInvoke, Ptr<FuncBase> func,
    const Ptr<Value>& thisValue, const std::vector<Value*>& args)
{
    auto instParamTys = oriInvoke.GetInstantiatedParamTypes();
    auto instRetTy = oriInvoke.GetInstantiatedRetType();
    std::vector<Value*> applyArgs{thisValue};
    applyArgs.insert(applyArgs.end(), args.begin(), args.end());
    auto apply = builder.CreateExpression<Apply>(instRetTy, func, applyArgs, oriInvoke.GetParent());
    instParamTys[0] = thisValue->GetType();
    auto thisType = builder.GetType<RefType>(thisValue->GetType());
    auto instParentCustomTy = thisValue->GetType();
    apply->SetInstantiatedFuncType(thisType, instParentCustomTy, std::move(instParamTys), *instRetTy);
    return apply;
}

Ptr<BinaryExpression> BuiltinOpCreateNewBinary(CHIRBuilder& builder, const Invoke& oriInvoke, ExprKind kind,
    const Ptr<Value>& thisValue, const std::vector<Value*>& args)
{
    CJC_ASSERT(args.size() == 1U);
    auto instRetTy = oriInvoke.GetInstantiatedRetType();
    auto parent = oriInvoke.GetParent();
    return builder.CreateExpression<BinaryExpression>(
        oriInvoke.GetDebugLocation(), instRetTy, kind, thisValue, args[0], parent);
}
}

bool Devirtualization::RewriteToBuiltinOp(CHIRBuilder& builder, const RewriteInfo& info, bool isDebug)
{
    auto invoke = info.invoke;
    auto func = info.realCallee->Get<WrappedRawMethod>() ? info.realCallee->Get<WrappedRawMethod>() : info.realCallee;

    ExprKind targetExprKind{ExprKind::INVALID};
    size_t operandsNum = 0;
    for (auto& it : COMPARABLE_FUNC_LISTS) {
        if (IsExpectedFunction(*func, it.funcInfo)) {
            targetExprKind = it.targetExprKind;
            operandsNum = it.operandsNum;
        }
    }
    auto args = invoke->GetArgs();
    if (targetExprKind == ExprKind::INVALID || args.size() != operandsNum - 1U) {
        return false;
    }
    for (auto& arg : args) {
        if (!arg->GetType()->IsPrimitive()) {
            return false;
        }
    }
    auto thisValue = invoke->GetObject();
    Expression* castExpr = nullptr;
    if (thisValue->IsLocalVar() && !thisValue->GetType()->IsPrimitive()) {
        auto expr = StaticCast<LocalVar*>(thisValue)->GetExpr();
        if (expr->GetExprKind() == ExprKind::BOX) {
            thisValue = expr->GetOperand(0);
            castExpr = expr;
        }
    }
    if (!thisValue->GetType()->IsPrimitive()) {
        return false;
    }
    Ptr<Expression> op;
    if (targetExprKind == ExprKind::APPLY) {
        op = BuiltinOpCreateNewApply(builder, *invoke, func, thisValue, args);
    } else {
        op = BuiltinOpCreateNewBinary(builder, *invoke, targetExprKind, thisValue, args);
    }
    invoke->ReplaceWith(*op);
    if (castExpr != nullptr && castExpr->GetResult()->GetUsers().empty()) {
        castExpr->RemoveSelfFromBlock();
    }
    if (isDebug) {
        std::string callName =
            targetExprKind == ExprKind::APPLY ? func->GetSrcCodeIdentifier() : op->GetExprKindName();
        std::string message = "[Devirtualization] The function call to " + invoke->GetMethodName() +
                              ToPosInfo(invoke->GetDebugLocation()) + " was optimized to builtin op " + callName + ".";
        std::cout << message << std::endl;
    }
    return true;
}

void Devirtualization::RewriteToApply(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfos, bool isDebug)
{
    for (auto rewriteInfo = rewriteInfos.rbegin(); rewriteInfo != rewriteInfos.rend(); ++rewriteInfo) {
        if (RewriteToBuiltinOp(builder, *rewriteInfo, isDebug)) {
            continue;
        }
        auto invoke = rewriteInfo->invoke;
        auto parent = invoke->GetParent();
        auto thisType = builder.GetType<RefType>(rewriteInfo->thisType);
        auto instParentCustomTy = rewriteInfo->thisType;
        auto instParamTys = invoke->GetInstantiatedParamTypes();
        if (rewriteInfo->thisType->IsBuiltinType()) {
            instParamTys[0] = builder.GetType<RefType>(builder.GetAnyTy());
        } else if (thisType->IsEqualOrSubTypeOf(*instParamTys[0], builder)) {
            instParamTys[0] = thisType;
        }
        auto instRetTy = invoke->GetInstantiatedRetType();
        auto args = invoke->GetOperands();
        auto typecastRes = TypeCastOrBoxIfNeeded(*args[0], *instParamTys[0], builder, *parent, INVALID_LOCATION);
        if (typecastRes != args[0]) {
            StaticCast<LocalVar*>(typecastRes)->GetExpr()->MoveBefore(invoke);
            args[0] = typecastRes;
        }
        auto apply = builder.CreateExpression<Apply>(instRetTy, rewriteInfo->realCallee, args, invoke->GetParent());
        rewriteInfo->newApply = apply;
        apply->SetDebugLocation(invoke->GetDebugLocation());
        apply->SetInstantiatedArgTypes(rewriteInfo->typeArgs);
        apply->SetInstantiatedFuncType(thisType, instParentCustomTy, std::move(instParamTys), *instRetTy);
        invoke->ReplaceWith(*apply);
        invoke->GetResult()->ReplaceWith(*apply->GetResult(), parent->GetParentBlockGroup());
        if (isDebug) {
            std::string message = "[Devirtualization] The function call to " + invoke->GetMethodName() +
                ToPosInfo(invoke->GetDebugLocation()) + " was optimized.";
            std::cout << message << std::endl;
        }
    }
}

const std::vector<Func*>& Devirtualization::GetFrozenInstFuns() const
{
    return frozenInstFuns;
}

void Devirtualization::AppendFrozenFuncState(const Func* func, std::unique_ptr<Results<TypeDomain>> analysisRes)
{
    frozenStates.emplace(func, std::move(analysisRes));
}

static std::string CreateInstFuncMangleName(const std::string& oriIdentifer, const Apply& apply)
{
    // 1. get type args
    std::vector<Type*> genericTypes;
    auto func = VirtualCast<FuncBase*>(apply.GetCallee());
    if (auto customDef = func->GetParentCustomTypeDef(); customDef != nullptr && customDef->IsGenericDef()) {
        auto funcInCustomType = apply.GetInstParentCustomTyOfCallee();
        while (funcInCustomType->IsRef()) {
            funcInCustomType = StaticCast<RefType*>(funcInCustomType)->GetBaseType();
        }
        genericTypes = funcInCustomType->GetTypeArgs();
    }
    auto funcArgs = apply.GetInstantiateArgs();
    if (!funcArgs.empty()) {
        genericTypes.insert(genericTypes.end(), funcArgs.begin(), funcArgs.end());
    }
    // 2. get mangle
    return CHIRMangling::GenerateInstantiateFuncMangleName(oriIdentifer, genericTypes);
}

void Devirtualization::InstantiateFuncIfPossible(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfoList)
{
    for (auto rewriteInfo = rewriteInfoList.rbegin(); rewriteInfo != rewriteInfoList.rend(); ++rewriteInfo) {
        auto callee = DynamicCast<Func*>(rewriteInfo->realCallee);
        if (callee == nullptr || !callee->IsInGenericContext() || callee->Get<WrappedRawMethod>() != nullptr) {
            continue;
        }
        auto apply = rewriteInfo->newApply;
        auto parameterType = apply->GetInstantiatedParamTypes();
        auto retType = apply->GetInstantiatedRetType();
        auto instFuncType = builder.GetType<FuncType>(parameterType, retType);

        if (instFuncType->IsGenericRelated()) {
            continue;
        }
        // 2. create new inst func if needed
        auto newId = CreateInstFuncMangleName(callee->GetIdentifierWithoutPrefix(), *apply);
        Func* newFunc;
        if (frozenInstFuncMap.count(newId) != 0) {
            newFunc = frozenInstFuncMap.at(newId);
        } else {
            newFunc = builder.CreateFunc(callee->GetDebugLocation(), instFuncType, newId,
                callee->GetSrcCodeIdentifier(), callee->GetRawMangledName(), callee->GetPackageName());
            
            newFunc->AppendAttributeInfo(callee->GetAttributeInfo());
            newFunc->DisableAttr(Attribute::GENERIC);
            if (!apply->GetInstantiateArgs().empty()) {
                newFunc->EnableAttr(Attribute::GENERIC_INSTANTIATED);
            }
            newFunc->Set<LinkTypeInfo>(Linkage::INTERNAL);

            auto oriBlockGroup = callee->GetBody();
            BlockGroupCopyHelper helper(builder);
            helper.GetInstMapFromApply(*apply);
            auto [newGroup, newBlockGroupRetValue] = helper.CloneBlockGroup(*oriBlockGroup, *newFunc);
            newFunc->InitBody(*newGroup);
            newFunc->SetReturnValue(*newBlockGroupRetValue);

            std::vector<Value*> args;
            CJC_ASSERT(parameterType.size() == callee->GetParams().size());
            std::unordered_map<Value*, Value*> paramMap;
            for (size_t i = 0; i < parameterType.size(); i++) {
                auto arg = builder.CreateParameter(parameterType[i], callee->GetParam(i)->GetDebugLocation(), *newFunc);
                args.push_back(arg);
                paramMap.emplace(callee->GetParam(i), arg);
            }
            helper.SubstituteValue(newGroup, paramMap);

            FixCastProblemAfterInst(newGroup, builder);
            frozenInstFuns.push_back(newFunc);
            frozenInstFuncMap[newId] = newFunc;
        }
        // replace apply callee with new inst func
        auto applyParent = apply->GetParent();
        auto instApply = builder.CreateExpression<Apply>(retType, newFunc, apply->GetArgs(), applyParent);
        instApply->SetInstantiatedFuncType(nullptr, nullptr, parameterType, *retType);
        instApply->SetDebugLocation(apply->GetDebugLocation());
        apply->ReplaceWith(*instApply);
    }
}

namespace {
void BuildOrphanTypeReplaceTable(
    const Cangjie::CHIR::Type* mayBeGeneric, std::unordered_map<const GenericType*, Type*>& replaceTable)
{
    if (auto genericType = Cangjie::DynamicCast<const GenericType*>(mayBeGeneric);
        genericType && genericType->orphanFlag) {
        CJC_ASSERT(genericType->GetUpperBounds().size() == 1);
        replaceTable.emplace(genericType, genericType->GetUpperBounds()[0]);
    } else {
        auto genericTypeArgs = mayBeGeneric->GetTypeArgs();
        for (size_t i = 0; i < genericTypeArgs.size(); ++i) {
            BuildOrphanTypeReplaceTable(genericTypeArgs[i], replaceTable);
        }
    }
}

FuncBase* FindFunctionInVtable(const ClassType* parentTy, const std::vector<VirtualFuncInfo>& infos,
    const Devirtualization::FuncSig& method, CHIRBuilder& builder)
{
    std::unordered_map<const GenericType*, Type*> parentReplaceTable;
    auto paramTypes = method.types;
    paramTypes.erase(paramTypes.begin());
    if (!parentTy->GetTypeArgs().empty()) {
        auto instParentTypeArgs = parentTy->GetTypeArgs();
        auto genericParentTypeArgs = parentTy->GetCustomTypeDef()->GetGenericTypeParams();
        for (size_t i = 0; i < genericParentTypeArgs.size(); ++i) {
            parentReplaceTable.emplace(genericParentTypeArgs[i], instParentTypeArgs[i]);
        }
    }

    for (auto& info : infos) {
        if (info.srcCodeIdentifier != method.name) {
            continue;
        }
        auto sigParamTys = info.typeInfo.sigType->GetParamTypes();
        if (sigParamTys.size() != paramTypes.size()) {
            continue;
        }
        if (info.typeInfo.methodGenericTypeParams.size() != method.typeArgs.size()) {
            continue;
        }
        bool isSigSame = true;
        std::unordered_map<const GenericType*, Type*> freeGenericReplaceTable;
        for (size_t i = 0; i < sigParamTys.size(); ++i) {
            BuildOrphanTypeReplaceTable(sigParamTys[i], freeGenericReplaceTable);
            BuildOrphanTypeReplaceTable(paramTypes[i], freeGenericReplaceTable);
        }
        auto& methodGenerics = info.typeInfo.methodGenericTypeParams;
        for (size_t i{0}; i < methodGenerics.size(); ++i) {
            freeGenericReplaceTable.emplace(methodGenerics[i], method.typeArgs[i]);
        }
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (isSigSame) {
                auto lhs = ReplaceRawGenericArgType(*sigParamTys[i], freeGenericReplaceTable, builder);
                auto rhs = ReplaceRawGenericArgType(*paramTypes[i], parentReplaceTable, builder);
                rhs = ReplaceRawGenericArgType(*rhs, freeGenericReplaceTable, builder);
                isSigSame = lhs == rhs;
            } else {
                break;
            }
        }
        if (!isSigSame) {
            continue;
        }
        return info.instance;
    }
    return nullptr;
}
} // namespace

bool Devirtualization::IsInstantiationOf(
    CHIRBuilder& builder, const GenericType* generic, const Type* instantiated) const
{
    if (generic->GetUpperBounds().empty()) {
        return true;
    }
    std::unordered_set<Type*> possibleParentTys;
    for (auto def : devirtFuncInfo.defsMap[instantiated]) {
        for (auto parentTy : def->GetSuperTypesInCurDef()) {
            auto inheritLists = parentTy->GetSuperTypesRecusively(builder);
            possibleParentTys.insert(inheritLists.begin(), inheritLists.end());
        }
    }
    for (auto upperBound : generic->GetUpperBounds()) {
        if (possibleParentTys.find(upperBound) == possibleParentTys.end()) {
            return false;
        }
    }

    return true;
}

bool Devirtualization::IsValidSubType(CHIRBuilder& builder, const Type* expected, Type* specific,
    std::unordered_map<const GenericType*, Type*>& replaceTable) const
{
    if (expected->GetTypeKind() != specific->GetTypeKind() && !expected->IsGeneric()) {
        return false;
    }
    if (expected->IsGeneric()) {
        auto generic = Cangjie::StaticCast<const GenericType*>(expected);
        if (IsInstantiationOf(builder, generic, specific)) {
            replaceTable.emplace(generic, specific);
            return true;
        }
        return false;
    }
    if (expected->IsNominal()) {
        const CustomType* specificCustomTy = Cangjie::StaticCast<const CustomType*>(specific);
        if (!specificCustomTy->IsEqualOrSubTypeOf(*expected, builder)) {
            return false;
        }
    }
    auto argsOfExpected = expected->GetTypeArgs();
    auto argsOfSpecific = specific->GetTypeArgs();
    if (argsOfExpected.size() != argsOfSpecific.size()) {
        return false;
    }
    for (size_t i = 0; i < argsOfExpected.size(); ++i) {
        if (!IsValidSubType(
            builder, argsOfExpected[i]->StripAllRefs(), argsOfSpecific[i]->StripAllRefs(), replaceTable)) {
            return false;
        }
    }
    return true;
}

std::pair<FuncBase*, Type*> Devirtualization::FindRealCallee(
    CHIRBuilder& builder, const TypeValue* typeState, const FuncSig& method) const
{
    auto typeStateKind = typeState->GetTypeKind();
    auto specificType = typeState->GetSpecificType();
    if (!specificType->IsClass() || typeStateKind == DevirtualTyKind::EXACTLY) {
        std::vector<CustomTypeDef*> extendsOrImplements{};
        if (auto customType = DynamicCast<const CustomType*>(specificType)) {
            extendsOrImplements = devirtFuncInfo.defsMap[customType->GetCustomTypeDef()->GetType()];
        } else {
            extendsOrImplements = devirtFuncInfo.defsMap[specificType];
        }

        FuncBase* target = nullptr;
        for (auto def : extendsOrImplements) {
            auto [typeMatched, replaceTable] = def->GetType()->CalculateGenericTyMapping(*specificType);
            if (!typeMatched) {
                continue;
            }
            auto funcType = builder.GetType<FuncType>(method.types, builder.GetUnitTy());
            auto res = def->GetFuncIndexInVTable(method.name, *funcType, false, replaceTable, method.typeArgs, builder);
            if (res.instance != nullptr) {
                target = res.instance;
                break;
            }
        }
        CJC_NULLPTR_CHECK(target);
        return {target, specificType};
    } else {
        // The specific type is an interface or a class, and the state kind is SUBCLASS_OF.
        ClassType* specificType1 = StaticCast<ClassType*>(typeState->GetSpecificType());
        std::pair<FuncBase*, Type*> res{nullptr, nullptr};
        CollectCandidates(builder, specificType1, res, method);
        return res;
    }
}

bool IsFromCoreIterator(const ClassDef& def)
{
    if (def.GetSuperTypesInCurDef().size() != 1U) {
        return false;
    }
    auto parentDef = def.GetSuperTypesInCurDef()[0]->GetClassDef();
    // need more accurate range which cannot be inherit.
    return def.GetPackageName().substr(0, 4U) == "std." && parentDef->GetPackageName() == CORE_PACKAGE_NAME &&
           parentDef->GetIdentifier() == "@_CNat8IteratorIG_E";
}

static bool IsOpenClass(const ClassDef& def)
{
    if (def.IsInterface() || def.IsAbstract()) {
        return true;
    }
    return def.TestAttr(Attribute::VIRTUAL);
}

bool Devirtualization::GetSpecificCandidates(
    CHIRBuilder& builder, ClassType& specific, std::pair<FuncBase*, Type*>& res, const FuncSig& method) const
{
    auto specificDef = specific.GetClassDef();
    if (IsOpenClass(*specificDef) && !specificDef->TestAttr(Attribute::PRIVATE) &&
        !IsFromCoreIterator(*specificDef)) {
        return false;
    }
    if (specificDef->IsAbstract() || specificDef->IsInterface()) {
        return true;
    }
    auto extendsOrImplements = devirtFuncInfo.defsMap[&specific];
    for (auto oriDef : extendsOrImplements) {
        auto genericDef = oriDef->GetGenericDecl() != nullptr ? oriDef->GetGenericDecl() : oriDef;
        for (auto [parentTy, infos] : genericDef->GetVTable()) {
            if (auto target = FindFunctionInVtable(parentTy, infos, method, builder)) {
                res = {target, &specific};
                return true;
            }
        }
    }
    return true;
}

void Devirtualization::CollectCandidates(
    CHIRBuilder& builder, ClassType* specific, std::pair<FuncBase*, Type*>& res, const FuncSig& method) const
{
    // 1. Get candidate from this type
    if (!GetSpecificCandidates(builder, *specific, res, method)) {
        return;
    }
    if (!IsOpenClass(*specific->GetClassDef())) {
        return;
    }
    auto& subtypeMap = devirtFuncInfo.GetSubtypeMap();
    auto it = subtypeMap.find(specific->GetClassDef());
    if (it == subtypeMap.end()) {
        // return if has no subtype
        return;
    }
    // 2. Get candidate from subtypes
    for (auto& inheritInfo : it->second) {
        auto expected = inheritInfo.parentInstType;
        std::unordered_map<const GenericType*, Type*> replaceTable;
        if (!IsValidSubType(builder, expected, specific, replaceTable)) {
            continue;
        }
        auto subtype = ReplaceRawGenericArgType(*(inheritInfo.subInstType), replaceTable, builder);
        auto subtypeClass = DynamicCast<ClassType*>(subtype);
        if (!subtypeClass ||
            (!subtypeClass->GetClassDef()->IsInterface() &&
                !subtypeClass->GetClassDef()->TestAttr(Attribute::ABSTRACT))) {
            auto extendsOrImplements = devirtFuncInfo.defsMap[subtypeClass];
            for (auto oriDef : extendsOrImplements) {
                auto def = oriDef->GetGenericDecl() != nullptr ? oriDef->GetGenericDecl() : oriDef;
                for (auto [parentTy, infos] : def->GetVTable()) {
                    if (!expected->IsEqualOrSubTypeOf(*parentTy, builder)) {
                        continue;
                    }
                    if (auto target = FindFunctionInVtable(parentTy, infos, method, builder)) {
                        if (res.first == nullptr) {
                            res = {target, subtypeClass};
                        } else if (res.first != target) {
                            res = {nullptr, nullptr};
                            return;
                        }
                    }
                }
            }
        }
        if (subtypeClass) {
            CollectCandidates(builder, subtypeClass, res, method);
        }
    }
}

bool Devirtualization::CheckFuncHasInvoke(const BlockGroup& bg)
{
    std::vector<Block*> blocks = bg.GetBlocks();
    for (auto bb : blocks) {
        auto exprs = bb->GetNonTerminatorExpressions();
        for (size_t i = 0; i < exprs.size(); ++i) {
            if (exprs[i]->GetExprKind() == ExprKind::LAMBDA) {
                if (CheckFuncHasInvoke(*StaticCast<Lambda*>(exprs[i])->GetLambdaBody())) {
                    return true;
                }
            }
            if (exprs[i]->GetExprKind() == ExprKind::INVOKE) {
                return true;
            }
        }
    }
    return false;
}

std::vector<Func*> Devirtualization::CollectContainInvokeExprFuncs(const Ptr<const Package>& package)
{
    std::vector<Func*> funcs;
    // Collect functions that contain the invoke statement.
    for (auto func : package->GetGlobalFuncs()) {
        if (CheckFuncHasInvoke(*func->GetBody())) {
            funcs.emplace_back(func);
        }
    }
    return funcs;
}
}