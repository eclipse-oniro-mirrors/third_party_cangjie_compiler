// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/Sema/MockSupportManager.h"

#include "TypeCheckUtil.h"
#include "Desugar/AfterTypeCheck.h"

#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/Sema/TestManager.h"

namespace Cangjie {

using namespace AST;
using namespace TypeCheckUtil;
using namespace Cangjie::Sema::Desugar::AfterTypeCheck;

OwnedPtr<RefExpr> CreateThisRef(ClassDecl& targetClass)
{
    auto thisRef = CreateRefExpr(SrcIdentifier{"this"}, targetClass.ty);
    thisRef->ref.target = &targetClass;
    return thisRef;
}

bool IsFieldOrVariable(AccessorKind kind)
{
    return kind == AccessorKind::FIELD_GETTER || kind == AccessorKind::FIELD_SETTER ||
        kind == AccessorKind::STATIC_FIELD_GETTER || kind == AccessorKind::STATIC_FIELD_SETTER ||
        kind == AccessorKind::TOP_LEVEL_VARIABLE_GETTER || kind == AccessorKind::TOP_LEVEL_VARIABLE_SETTER;
}

bool IsStaticField(AccessorKind kind)
{
    return kind == AccessorKind::STATIC_FIELD_GETTER || kind == AccessorKind::STATIC_FIELD_SETTER;
}

bool IsTopLevelField(AccessorKind kind)
{
    return kind == AccessorKind::TOP_LEVEL_VARIABLE_GETTER || kind == AccessorKind::TOP_LEVEL_VARIABLE_SETTER;
}

MockSupportManager::MockSupportManager(TypeManager& typeManager, const Ptr<MockUtils> mockUtils)
    : typeManager(typeManager),
      mockUtils(mockUtils)
{}

Ptr<Expr> ExtractLastDesugaredExpr(Expr& expr)
{
    auto lastDesugaredExpr = &expr;

    while (lastDesugaredExpr->desugarExpr != nullptr) {
        lastDesugaredExpr = lastDesugaredExpr->desugarExpr.get();
    }

    return lastDesugaredExpr;
}

bool MockSupportManager::IsDeclOpenToMock(const Decl& decl)
{
    return decl.TestAttr(Attribute::OPEN) && decl.TestAttr(Attribute::OPEN_TO_MOCK);
}

bool MockSupportManager::DoesClassLikeSupportMocking(ClassLikeDecl& classLikeToCheck)
{
    if (Is<InterfaceDecl>(classLikeToCheck) || classLikeToCheck.TestAttr(Attribute::MOCK_SUPPORTED)) {
        return true;
    }

    if (!classLikeToCheck.TestAttr(Attribute::OPEN) && !classLikeToCheck.TestAttr(Attribute::ABSTRACT)) {
        return false;
    }

    for (auto& superDecl : classLikeToCheck.GetAllSuperDecls()) {
        for (auto& member : superDecl->GetMemberDecls()) {
            if (member->TestAttr(Attribute::CONSTRUCTOR)) {
                continue;
            }
            if (!member->TestAttr(Attribute::OPEN) && !member->TestAttr(Attribute::ABSTRACT)) {
                return false;
            }
        }
    }

    return true;
}

void MockSupportManager::MakeOpenToMockIfNeeded(Decl& decl)
{
    if (!decl.TestAttr(Attribute::OPEN) && !decl.TestAttr(Attribute::ABSTRACT)) {
        decl.EnableAttr(Attribute::OPEN);
        decl.EnableAttr(Attribute::OPEN_TO_MOCK);
    }
}

// Here we mark only generic decls and package decl because they are exported early,
// before type instantiation stage where we do all mocking preparations
void MockSupportManager::MarkNodeMockSupportedIfNeeded(Node& node)
{
    auto decl = As<ASTKind::DECL>(&node);

    if (!decl) {
        return;
    }

    if (Is<ClassDecl>(decl)) {
        decl->EnableAttr(Attribute::MOCK_SUPPORTED);
        MakeOpenToMockIfNeeded(*decl);
    } else if ((Is<FuncDecl>(decl) || Is<PropDecl>(decl)) &&
        !MockUtils::IsMockAccessorRequired(*decl) &&
        !decl->IsStaticOrGlobal() &&
        !decl->TestAttr(Attribute::CONSTRUCTOR)
    ) {
        MakeOpenToMockIfNeeded(*decl);
        if (auto propMember = As<ASTKind::PROP_DECL>(decl); propMember) {
            MakeOpenToMockIfNeeded(*GetUsableGetterForProperty(*propMember));
            if (propMember->isVar) {
                MakeOpenToMockIfNeeded(*GetUsableSetterForProperty(*propMember));
            }
        }
    } else if (Is<FuncDecl>(decl) && decl->IsStaticOrGlobal() && decl->TestAttr(AST::Attribute::GENERIC)) {
        if (!(decl->outerDecl && (decl->outerDecl->TestAttr(Attribute::GENERIC) || decl->outerDecl->genericDecl))) {
            decl->EnableAttr(Attribute::MOCK_SUPPORTED);
        }
    }
}

void MockSupportManager::MarkMockAccessorWithAttributes(Decl& decl)
{
    decl.EnableAttr(Attribute::OPEN);
    decl.EnableAttr(Attribute::PUBLIC);
    decl.EnableAttr(Attribute::GENERATED_TO_MOCK);
    decl.EnableAttr(Attribute::COMPILER_ADD);
    decl.EnableAttr(Attribute::IN_CLASSLIKE);
}

void MockSupportManager::PrepareStaticDecls(std::vector<Ptr<Decl>> decls)
{
    for (auto& decl : decls) {
        if (auto propDecl = As<ASTKind::PROP_DECL>(decl)) {
            PrepareStaticDecl(*GetUsableGetterForProperty(*propDecl));
            if (propDecl->isVar) {
                PrepareStaticDecl(*GetUsableSetterForProperty(*propDecl));
            }
            propDecl->EnableAttr(Attribute::MOCK_SUPPORTED);
            continue;
        }

        auto funcDecl = As<ASTKind::FUNC_DECL>(decl);
        CJC_NULLPTR_CHECK(funcDecl);

        if (funcDecl->TestAttr(Attribute::FOREIGN)) {
            auto wrapperDecl = CreateForeignFunctionAccessorDecl(*funcDecl);
            PrepareStaticDecl(*wrapperDecl);
            generatedMockDecls.emplace(std::move(wrapperDecl));
        } else {
            if (funcDecl->outerDecl &&
                (funcDecl->outerDecl->TestAttr(Attribute::GENERIC) || funcDecl->outerDecl->genericDecl)) {
                return;
            }

            PrepareStaticDecl(*funcDecl);

            if (auto instantiatedDecls = mockUtils->TryGetInstantiatedDecls(*funcDecl)) {
                for (auto& iDecl : *instantiatedDecls) {
                    PrepareStaticDecl(*iDecl);
                }
            }
        }
    }
}

void MockSupportManager::CollectStaticDeclsToPrepare(Decl& decl, std::vector<Ptr<Decl>>& decls)
{
    for (auto& member : decl.GetMemberDecls()) {
        if (auto propDecl = As<ASTKind::PROP_DECL>(member); propDecl) {
            decls.emplace_back(propDecl);
        } else {
            CollectStaticDeclsToPrepare(*member, decls);
        }
    }

    if (!Is<FuncDecl>(decl) || !decl.IsStaticOrGlobal() ||
        decl.TestAttr(Attribute::PRIVATE) || decl.TestAttr(Attribute::MAIN_ENTRY) ||
        decl.TestAttr(Attribute::IN_EXTEND)
    ) {
        return;
    }

    decls.emplace_back(&decl);
}

std::vector<OwnedPtr<MatchCase>> MockSupportManager::GenerateHandlerMatchCases(
    const FuncDecl& funcDecl, OwnedPtr<EnumPattern> optionFuncTyPattern, OwnedPtr<RefExpr> varPatternRef)
{
    static const auto NOTHING_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_NOTHING);
    static const auto UNIT_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    auto handlerRetTy = typeManager.GetAnyTy();
    auto optionFuncRetTy = typeManager.GetEnumTy(*mockUtils->optionDecl, { handlerRetTy });
    std::vector<OwnedPtr<FuncArg>> handlerCallArgs {};
    handlerCallArgs.emplace_back(CreateFuncArg(mockUtils->WrapCallArgsIntoArray(funcDecl)));
    handlerCallArgs.emplace_back(CreateFuncArg(mockUtils->WrapCallTypeArgsIntoArray(funcDecl)));
    auto handlerCallExpr = CreateCallExpr(
        std::move(varPatternRef), std::move(handlerCallArgs), nullptr, optionFuncRetTy);
    handlerCallExpr->callKind = CallKind::CALL_FUNCTION_PTR;

    std::vector<OwnedPtr<MatchCase>> handlerResultCases;
    auto handlerResultPattern = MakeOwned<EnumPattern>();
    auto handlerResultPatternConstructor = LookupEnumMember(
        mockUtils->getInstantiatedDecl(*optionFuncRetTy->decl, {handlerRetTy}, nullptr), OPTION_VALUE_CTOR);
    handlerResultPattern->ty = RawStaticCast<FuncTy*>(handlerResultPatternConstructor->ty)->retTy;
    handlerResultPattern->constructor = mockUtils->CreateRefExprWithInstTys(
        *handlerResultPatternConstructor, {handlerRetTy}, OPTION_VALUE_CTOR, *(funcDecl.curFile));

    auto handlerResultVarPattern = CreateVarPattern(V_COMPILER, handlerRetTy);
    auto handlerResultRef = CreateRefExpr(*(handlerResultVarPattern->varDecl));
    handlerResultRef->ty = handlerRetTy;
    handlerResultPattern->patterns.emplace_back(std::move(handlerResultVarPattern));

    auto castTy = RawStaticCast<const FuncTy*>(funcDecl.ty)->retTy;
    auto castType = MockUtils::CreateType<Type>(castTy);
    auto varPatternForTypeCast = CreateVarPattern(V_COMPILER, castTy);
    auto varPatternForTypeCastRef = CreateRefExpr(*(varPatternForTypeCast->varDecl));
    varPatternForTypeCastRef->ty = castTy;
    varPatternForTypeCastRef->instTys.emplace_back(castTy);

    std::vector<OwnedPtr<MatchCase>> matchCasesTypeCast;

    auto retExprWithCastedType = CreateReturnExpr(std::move(varPatternForTypeCastRef));
    retExprWithCastedType->ty = NOTHING_TY;
    auto typePattern = CreateTypePattern(std::move(varPatternForTypeCast), std::move(castType), *handlerResultRef);
    typePattern->curFile = funcDecl.curFile;
    auto typeCastMatchCase = CreateMatchCase(std::move(typePattern), std::move(retExprWithCastedType));

    auto zeroValueRet = CreateReturnExpr(mockUtils->CreateZeroValue(castTy, *funcDecl.curFile));
    zeroValueRet->ty = NOTHING_TY;

    if (!castTy->IsNothing()) {
        // There is no valid cast from Any to Nothing
        matchCasesTypeCast.emplace_back(std::move(typeCastMatchCase));
    }
    matchCasesTypeCast.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(), std::move(zeroValueRet)));

    auto retExpr = CreateMatchExpr(std::move(handlerResultRef), std::move(matchCasesTypeCast), NOTHING_TY);

    handlerResultCases.emplace_back(CreateMatchCase(std::move(handlerResultPattern), std::move(retExpr)));
    handlerResultCases.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(), CreateUnitExpr(UNIT_TY)));

    std::vector<OwnedPtr<MatchCase>> handlerCases;
    handlerCases.emplace_back(
        CreateMatchCase(
            std::move(optionFuncTyPattern),
            CreateMatchExpr(std::move(handlerCallExpr), std::move(handlerResultCases), UNIT_TY)));
    handlerCases.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(), CreateUnitExpr(UNIT_TY)));
    return handlerCases;
}

void MockSupportManager::PrepareStaticDecl(Decl& decl)
{
    static const auto UNIT_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    auto funcDecl = As<ASTKind::FUNC_DECL>(&decl);
    if (!funcDecl) {
        return;
    }

    // Do not generate mock var for $test.entry function
    if (funcDecl->identifier == TEST_ENTRY_NAME) {
        return;
    }

    auto body = funcDecl->funcBody->body.get();
    if (!body) {
        return;
    }

    auto handlerRetTy = typeManager.GetAnyTy();
    auto optionFuncRetTy = typeManager.GetEnumTy(*mockUtils->optionDecl, { handlerRetTy });
    auto arrayTy = typeManager.GetStructTy(*mockUtils->arrayDecl, { typeManager.GetAnyTy() });
    auto toStrArrayTy = typeManager.GetStructTy(*mockUtils->arrayDecl, { mockUtils->toStringDecl->ty });
    auto funcTy = typeManager.GetFunctionTy({arrayTy, toStrArrayTy}, optionFuncRetTy);
    auto optionFuncTy = typeManager.GetEnumTy(*mockUtils->optionDecl, { funcTy });

    auto noneFuncTy = CreateRefExpr(
        *LookupEnumMember(
            mockUtils->getInstantiatedDecl(*optionFuncTy->decl, {optionFuncTy}, nullptr), OPTION_NONE_CTOR));
    noneFuncTy->ty = optionFuncTy;

    Ptr<VarDecl> varDecl = nullptr;
    if (funcDecl->genericDecl) {
        auto& genericDecl = *funcDecl->genericDecl;
        if (auto it = genericMockVarsDecls.find(&genericDecl); it != genericMockVarsDecls.end()) {
            // The var was already generated, but not yet written to file
            varDecl = it->second;
        } else {
            varDecl = As<ASTKind::VAR_DECL>(
                mockUtils->FindMockGlobalDecl(genericDecl, mockUtils->Mangle(genericDecl)));
        }

        CJC_ASSERT(varDecl);
    } else {
        auto varMangledName = mockUtils->Mangle(decl);
        auto newVarDecl = CreateVarDecl(varMangledName + MockUtils::mockAccessorSuffix, std::move(noneFuncTy), nullptr);
        newVarDecl->curFile = decl.curFile;
        newVarDecl->isVar = true;
        newVarDecl->EnableAttr(Attribute::PUBLIC);
        newVarDecl->EnableAttr(Attribute::GLOBAL);
        newVarDecl->fullPackageName = decl.fullPackageName;

        varDecl = newVarDecl.get();
        generatedMockDecls.insert(std::move(newVarDecl));
        genericMockVarsDecls.emplace(&decl, varDecl);
    }

    if (mockUtils->isInstantiationEnabled && funcDecl->TestAttr(Attribute::GENERIC)) {
        return;
    }

    auto varDeclRef = CreateRefExpr(*varDecl);
    varDeclRef->ty = optionFuncTy;

    auto optionFuncTyPattern = MakeOwned<EnumPattern>();
    auto optionFuncTyPatternConstructor = LookupEnumMember(
        mockUtils->getInstantiatedDecl(*optionFuncTy->decl, {funcTy}, nullptr), OPTION_VALUE_CTOR);
    optionFuncTyPattern->ty = RawStaticCast<FuncTy*>(optionFuncTyPatternConstructor->ty)->retTy;
    optionFuncTyPattern->constructor = mockUtils->CreateRefExprWithInstTys(
        *optionFuncTyPatternConstructor, {funcTy}, OPTION_VALUE_CTOR, *(decl.curFile));

    auto optionFuncTyVarPattern = CreateVarPattern(V_COMPILER, funcTy);
    auto varPatternRef = CreateRefExpr(*optionFuncTyVarPattern->varDecl.get());
    varPatternRef->ty = funcTy;
    optionFuncTyPattern->patterns.emplace_back(std::move(optionFuncTyVarPattern));

    auto handlerCases = GenerateHandlerMatchCases(
        *funcDecl, std::move(optionFuncTyPattern), std::move(varPatternRef));
    auto handlerMatch = CreateMatchExpr(std::move(varDeclRef), std::move(handlerCases), UNIT_TY);
    handlerMatch->curFile = funcDecl->curFile;
    mockUtils->instantiate(*handlerMatch);
    body->body.push_back(std::move(handlerMatch));
    std::rotate(body->body.rbegin(), body->body.rbegin() + 1, body->body.rend());

    decl.EnableAttr(Attribute::MOCK_SUPPORTED);
}

void MockSupportManager::GenerateVarDeclAccessors(VarDecl& fieldDecl, AccessorKind getterKind, AccessorKind setterKind)
{
    generatedMockDecls.insert(GenerateVarDeclAccessor(fieldDecl, getterKind));
    if (fieldDecl.isVar) {
        generatedMockDecls.insert(GenerateVarDeclAccessor(fieldDecl, setterKind));
    }
    fieldDecl.EnableAttr(Attribute::MOCK_SUPPORTED);
}

void MockSupportManager::GenerateSpyCallMarker(Package& package)
{
    if (package.files.size() == 0) {
        return;
    }

    static const auto BOOL_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto type = MockUtils::CreateType<PrimitiveType>(BOOL_TY);
    type->kind = TypeKind::TYPE_BOOLEAN;
    type->str = BOOL_TY->String();
    auto varDecl = CreateVarDecl(
        MockUtils::spyCallMarkerVarName + MockUtils::mockAccessorSuffix,
        CreateLitConstExpr(LitConstKind::BOOL, "false", BOOL_TY, true),
        std::move(type));
    varDecl->curFile = package.files[0].get();
    varDecl->isVar = true;
    varDecl->EnableAttr(Attribute::PUBLIC);
    varDecl->EnableAttr(Attribute::GLOBAL);
    varDecl->fullPackageName = package.fullPackageName;
    varDecl->TestAttr(Attribute::GENERATED_TO_MOCK);

    generatedMockDecls.insert(std::move(varDecl));
}

Ptr<Decl> MockSupportManager::GenerateSpiedObjectVar(const Decl& decl)
{
    auto optionDeclTy = typeManager.GetEnumTy(*mockUtils->optionDecl, { typeManager.GetAnyTy() });
    auto noneRef = CreateRefExpr(*LookupEnumMember(optionDeclTy->decl, OPTION_NONE_CTOR));
    noneRef->ty = optionDeclTy;

    auto varDecl = CreateVarDecl(
        MockUtils::spyObjVarName + "$" + mockUtils->mangler.Mangle(decl) + MockUtils::mockAccessorSuffix,
        std::move(noneRef),
        MockUtils::CreateType<RefType>(optionDeclTy));
    varDecl->curFile = decl.curFile;
    varDecl->isVar = true;
    varDecl->EnableAttr(Attribute::PUBLIC);
    varDecl->EnableAttr(Attribute::GLOBAL);
    varDecl->fullPackageName = decl.fullPackageName;
    varDecl->TestAttr(Attribute::GENERATED_TO_MOCK);

    auto varRef = varDecl.get();

    generatedMockDecls.insert(std::move(varDecl));

    return varRef;
}

void MockSupportManager::GenerateSpyCallHandler(FuncDecl& funcDecl, Decl& spiedObjectDecl)
{
    static const auto BOOL_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    static const auto UNIT_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    if (funcDecl.TestAttr(Attribute::CONSTRUCTOR) || funcDecl.TestAttr(Attribute::STATIC)) {
        return;
    }
    auto body = funcDecl.funcBody->body.get();
    if (!body || MockUtils::IsMockAccessor(funcDecl)) {
        return;
    }

    auto spiedObjOptionTy = StaticCast<EnumTy*>(spiedObjectDecl.ty);
    auto spiedObjTy = spiedObjOptionTy->typeArgs[0];
    auto optionSpiedObjTyPattern = MakeOwned<EnumPattern>();
    auto optionSpiedObjPatternConstructor = LookupEnumMember(
        mockUtils->getInstantiatedDecl(*spiedObjOptionTy->decl, {spiedObjTy}, nullptr), OPTION_VALUE_CTOR);
    optionSpiedObjTyPattern->ty = RawStaticCast<FuncTy*>(optionSpiedObjPatternConstructor->ty)->retTy;
    optionSpiedObjTyPattern->constructor = mockUtils->CreateRefExprWithInstTys(
        *optionSpiedObjPatternConstructor, {spiedObjTy}, OPTION_VALUE_CTOR, *(funcDecl.curFile));

    Ptr<Decl> spyCallMarker = nullptr;
    for (auto& mockDecl : generatedMockDecls) {
        if (mockDecl->identifier == MockUtils::spyCallMarkerVarName + MockUtils::mockAccessorSuffix) {
            spyCallMarker = mockDecl;
            break;
        }
    }

    if (!spyCallMarker) {
        return;
    }

    std::vector<OwnedPtr<FuncArg>> callBaseArgs;
    for (auto& param : funcDecl.funcBody->paramLists[0]->params) {
        callBaseArgs.emplace_back(CreateFuncArg(CreateRefExpr(*param)));
    }
    
    auto castTy = funcDecl.outerDecl->ty;
    auto castType = MockUtils::CreateType<RefType>(castTy);
    auto varPatternForTypeCast = CreateVarPattern(V_COMPILER, castTy);
    auto varPatternForTypeCastRef = CreateRefExpr(*(varPatternForTypeCast->varDecl));
    varPatternForTypeCastRef->instTys.emplace_back(castTy);

    auto memberAccessFuncBaseExpr = CreateMemberAccess(std::move(varPatternForTypeCastRef), funcDecl);

    if (auto& genericInfo = funcDecl.funcBody->generic; genericInfo) {
        for (auto& typeParam : genericInfo->typeParameters) {
            memberAccessFuncBaseExpr->instTys.emplace_back(typeParam->ty);
        }
    }
    auto callMockedMember = MakeOwned<CallExpr>();
    callMockedMember->ty = RawStaticCast<const FuncTy*>(memberAccessFuncBaseExpr->ty)->retTy;
    callMockedMember->resolvedFunction = RawStaticCast<FuncDecl*>(memberAccessFuncBaseExpr->target);
    callMockedMember->baseFunc = std::move(memberAccessFuncBaseExpr);
    callMockedMember->args = std::move(callBaseArgs);
    callMockedMember->callKind = CallKind::CALL_DECLARED_FUNCTION;
    callMockedMember->curFile = funcDecl.curFile;

    std::vector<OwnedPtr<Node>> nodes {};
    auto trueLit = CreateLitConstExpr(LitConstKind::BOOL, "true", BOOL_TY);
    trueLit->curFile = funcDecl.curFile;
    auto trueSpyCallMarkerAssign =
        CreateAssignExpr(CreateRefExpr(*spyCallMarker), std::move(trueLit), UNIT_TY);
    trueSpyCallMarkerAssign->curFile = funcDecl.curFile;
    trueSpyCallMarkerAssign->EnableAttr(Attribute::GENERATED_TO_MOCK);
    auto falseLit = CreateLitConstExpr(LitConstKind::BOOL, "false", BOOL_TY);
    falseLit->curFile = funcDecl.curFile;
    auto falseSpyCallMarkerAssign =
        CreateAssignExpr(CreateRefExpr(*spyCallMarker), std::move(falseLit), UNIT_TY);
    falseSpyCallMarkerAssign->curFile = funcDecl.curFile;
    falseSpyCallMarkerAssign->EnableAttr(Attribute::GENERATED_TO_MOCK);

    auto spiedObjVarPattern = CreateVarPattern(V_COMPILER, spiedObjTy);
    auto spiedObjVarRef = CreateRefExpr(*spiedObjVarPattern->varDecl.get());
    optionSpiedObjTyPattern->patterns.emplace_back(std::move(spiedObjVarPattern));

    auto callMockedMemberResult = CreateVarDecl(
        "callBaseResult" + MockUtils::mockAccessorSuffix, std::move(callMockedMember), nullptr);
    callMockedMemberResult->curFile = funcDecl.curFile;
    callMockedMemberResult->fullPackageName = funcDecl.fullPackageName;
    auto callMockedMemberResultRef = CreateRefExpr(*callMockedMemberResult);

    std::vector<OwnedPtr<MatchCase>> matchCasesTypeCast;

    auto typeCastMatchCase = CreateMatchCase(
        CreateTypePattern(std::move(varPatternForTypeCast), std::move(castType), *spiedObjVarRef),
        std::move(trueSpyCallMarkerAssign));
    typeCastMatchCase->exprOrDecls->body.emplace_back(std::move(callMockedMemberResult));
    typeCastMatchCase->exprOrDecls->body.emplace_back(std::move(falseSpyCallMarkerAssign));
    typeCastMatchCase->exprOrDecls->body.emplace_back(
        CreateReturnExpr(std::move(callMockedMemberResultRef), funcDecl.funcBody));

    matchCasesTypeCast.emplace_back(std::move(typeCastMatchCase));
    matchCasesTypeCast.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(), CreateUnitExpr(UNIT_TY)));

    auto typeCastMatch = CreateMatchExpr(std::move(spiedObjVarRef), std::move(matchCasesTypeCast), UNIT_TY);

    std::vector<OwnedPtr<MatchCase>> handlerCases;
    handlerCases.emplace_back(CreateMatchCase(std::move(optionSpiedObjTyPattern), std::move(typeCastMatch)));
    handlerCases.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(), CreateUnitExpr(UNIT_TY)));

    auto handlerMatch = CreateMatchExpr(CreateRefExpr(spiedObjectDecl), std::move(handlerCases), UNIT_TY);
    handlerMatch->curFile = funcDecl.curFile;

    auto falseLitBackCall = CreateLitConstExpr(LitConstKind::BOOL, "false", BOOL_TY);
    falseLitBackCall->curFile = funcDecl.curFile;
    auto falseSpyCallMarkerAssignBackCall = CreateAssignExpr(
        CreateRefExpr(*spyCallMarker), std::move(falseLitBackCall), UNIT_TY);
    falseSpyCallMarkerAssignBackCall->curFile = funcDecl.curFile;
    falseSpyCallMarkerAssignBackCall->EnableAttr(Attribute::GENERATED_TO_MOCK);
    std::vector<OwnedPtr<MatchCase>> callMarkerCases;
    OwnedPtr<ConstPattern> truePattern = MakeOwned<ConstPattern>();
    truePattern->literal = CreateLitConstExpr(LitConstKind::BOOL, "true", BOOL_TY, true);
    truePattern->ty = BOOL_TY;
    callMarkerCases.emplace_back(CreateMatchCase(
        std::move(truePattern),
        std::move(falseSpyCallMarkerAssignBackCall)));

    OwnedPtr<ConstPattern> falsePattern = MakeOwned<ConstPattern>();
    falsePattern->ty = BOOL_TY;
    falsePattern->literal = CreateLitConstExpr(LitConstKind::BOOL, "false", BOOL_TY, true);

    callMarkerCases.emplace_back(CreateMatchCase(
        std::move(falsePattern),
        std::move(handlerMatch)));

    auto spyCallMarkerMatch = CreateMatchExpr(CreateRefExpr(*spyCallMarker), std::move(callMarkerCases), UNIT_TY);

    mockUtils->instantiate(*spyCallMarkerMatch);
    body->body.push_back(std::move(spyCallMarkerMatch));
    std::rotate(body->body.rbegin(), body->body.rbegin() + 1, body->body.rend());
}

void MockSupportManager::PrepareToSpy(Decl& decl)
{
    auto classLikeDecl = As<ASTKind::CLASS_LIKE_DECL>(&decl);

    if (!classLikeDecl || (!classLikeDecl->TestAttr(Attribute::MOCK_SUPPORTED) && !Is<InterfaceDecl>(classLikeDecl))) {
        return;
    }

    auto spiedObjectDecl = GenerateSpiedObjectVar(decl);

    for (auto& member : classLikeDecl->GetMemberDecls()) {
        if (auto funcDecl = As<ASTKind::FUNC_DECL>(member); funcDecl) {
            GenerateSpyCallHandler(*funcDecl, *spiedObjectDecl);
        }
    }
}

void MockSupportManager::GenerateAccessors(Decl& decl)
{
    if (auto varDecl = As<ASTKind::VAR_DECL>(&decl); varDecl && varDecl->TestAttr(Attribute::GLOBAL)) {
        GenerateVarDeclAccessors(
            *varDecl, AccessorKind::TOP_LEVEL_VARIABLE_GETTER, AccessorKind::TOP_LEVEL_VARIABLE_SETTER);
        return;
    }

    auto classDecl = As<ASTKind::CLASS_DECL>(&decl);

    if (!classDecl) {
        return;
    }

    for (auto& member : classDecl->GetMemberDecls()) {
        if (member->TestAttr(Attribute::CONSTRUCTOR)) {
            continue;
        }
        if (member->TestAttr(Attribute::STATIC)) {
            if (auto fieldDecl = As<ASTKind::VAR_DECL>(member.get()); fieldDecl && !Is<PropDecl>(member.get())) {
                GenerateVarDeclAccessors(
                    *fieldDecl, AccessorKind::STATIC_FIELD_GETTER, AccessorKind::STATIC_FIELD_SETTER);
            }
            continue;
        }
        if (auto funcDecl = As<ASTKind::FUNC_DECL>(member); funcDecl && funcDecl->IsFinalizer()) {
            continue;
        }
        if (!MockUtils::IsMockAccessorRequired(*member)) {
            continue;
        }

        if (auto propDecl = As<ASTKind::PROP_DECL>(member.get()); propDecl) {
            generatedMockDecls.insert(GeneratePropAccessor(*propDecl));
        } else if (auto methodDecl = As<ASTKind::FUNC_DECL>(member.get()); methodDecl) {
            if (auto instantiatedDecls = mockUtils->TryGetInstantiatedDecls(*methodDecl)) {
                for (auto& instantiatedDecl : *instantiatedDecls) {
                    generatedMockDecls.insert(GenerateFuncAccessor(*RawStaticCast<FuncDecl*>(instantiatedDecl)));
                }
            } else {
                generatedMockDecls.insert(GenerateFuncAccessor(*RawStaticCast<FuncDecl*>(methodDecl)));
            }
        } else if (auto fieldDecl = As<ASTKind::VAR_DECL>(member.get()); fieldDecl) {
            GenerateVarDeclAccessors(*fieldDecl, AccessorKind::FIELD_GETTER, AccessorKind::FIELD_SETTER);
        }
    }
}

OwnedPtr<FuncDecl> MockSupportManager::GenerateFuncAccessor(FuncDecl& methodDecl) const
{
    auto outerClassDecl = As<ASTKind::CLASS_DECL>(methodDecl.outerDecl);
    CJC_ASSERT(outerClassDecl);

    auto retTy = RawStaticCast<const FuncTy*>(methodDecl.ty)->retTy;
    OwnedPtr<FuncDecl> methodAccessor = ASTCloner::Clone(Ptr(&methodDecl));

    mockUtils->AddGenericIfNeeded(methodDecl, *methodAccessor);

    std::vector<Ptr<Ty>> typeParamTys;
    auto memberAccessOriginal = CreateRefExpr(methodDecl);

    if (auto& generic = methodAccessor->funcBody->generic; generic) {
        for (auto& typeParam : generic->typeParameters) {
            typeParam->outerDecl = methodAccessor;
            typeParamTys.emplace_back(typeParam->ty);
            memberAccessOriginal->instTys.emplace_back(typeParam->ty);
        }
    }

    auto typeSubst = GenerateTypeMapping(methodDecl, typeParamTys);

    methodAccessor->ty = typeManager.GetInstantiatedTy(methodDecl.ty, typeSubst);
    memberAccessOriginal->ty = methodAccessor->ty;

    std::vector<OwnedPtr<FuncArg>> mockedMethodArgRefs {};
    for (auto& param : methodAccessor->funcBody->paramLists[0]->params) {
        param->ty = typeManager.GetInstantiatedTy(param->ty, typeSubst);
        param->outerDecl = methodAccessor.get();
        mockedMethodArgRefs.emplace_back(CreateFuncArg(CreateRefExpr(*param)));
    }

    auto callOriginalMethod = MakeOwned<CallExpr>();
    callOriginalMethod->ty = typeManager.GetInstantiatedTy(retTy, typeSubst);
    callOriginalMethod->resolvedFunction = &methodDecl;
    callOriginalMethod->baseFunc = std::move(memberAccessOriginal);
    callOriginalMethod->args = std::move(mockedMethodArgRefs);
    callOriginalMethod->callKind = CallKind::CALL_DECLARED_FUNCTION;
    callOriginalMethod->curFile = methodDecl.curFile;

    std::vector<OwnedPtr<Node>> mockedMethodBodyNodes;
    mockedMethodBodyNodes.emplace_back(CreateReturnExpr(std::move(callOriginalMethod), methodAccessor->funcBody.get()));
    methodAccessor->funcBody->body->body = std::move(mockedMethodBodyNodes);
    methodAccessor->funcBody->funcDecl = methodAccessor.get();
    methodAccessor->funcBody->ty = typeManager.GetInstantiatedTy(methodAccessor->funcBody->ty, typeSubst);
    methodAccessor->funcBody->body->ty = typeManager.GetInstantiatedTy(methodAccessor->funcBody->body->ty, typeSubst);
    methodAccessor->funcBody->retType->ty = typeManager.GetInstantiatedTy(
        methodAccessor->funcBody->retType->ty, typeSubst);

    MarkMockAccessorWithAttributes(*methodAccessor);
    methodAccessor->identifier = MockUtils::BuildMockAccessorIdentifier(methodDecl, AccessorKind::METHOD);
    methodAccessor->mangledName = mockUtils->mangler.Mangle(*methodAccessor);
    methodAccessor->linkage = Linkage::EXTERNAL;
    methodDecl.linkage = Linkage::EXTERNAL;

    return methodAccessor;
}

OwnedPtr<PropDecl> MockSupportManager::GeneratePropAccessor(PropDecl& propDecl) const
{
    OwnedPtr<PropDecl> propAccessor = ASTCloner::Clone(Ptr(&propDecl));
    auto outerClassDecl = As<ASTKind::CLASS_DECL>(propDecl.outerDecl);
    CJC_ASSERT(outerClassDecl);

    std::vector<OwnedPtr<FuncDecl>> accessorForGetters;
    std::vector<OwnedPtr<FuncDecl>> accessorForSetters;

    auto propGetter = GenerateFuncAccessor(*GetUsableGetterForProperty(propDecl));
    propGetter->propDecl = propAccessor.get();
    accessorForGetters.emplace_back(std::move(propGetter));
    if (propDecl.isVar) {
        auto propSetter = GenerateFuncAccessor(*GetUsableSetterForProperty(propDecl));
        propSetter->propDecl = propAccessor.get();
        accessorForSetters.emplace_back(std::move(propSetter));
    }
    propAccessor->getters = std::move(accessorForGetters);
    propAccessor->setters = std::move(accessorForSetters);
    propAccessor->identifier = MockUtils::BuildMockAccessorIdentifier(propDecl, AccessorKind::PROP);
    propAccessor->mangledName = mockUtils->mangler.Mangle(*propAccessor);
    MarkMockAccessorWithAttributes(*propAccessor);
    propAccessor->linkage = Linkage::EXTERNAL;

    return propAccessor;
}

std::vector<OwnedPtr<Node>> MockSupportManager::GenerateFieldGetterAccessorBody(
    VarDecl& fieldDecl, FuncBody& funcBody, AccessorKind kind) const
{
    OwnedPtr<Expr> retExpr;

    if (kind == AccessorKind::TOP_LEVEL_VARIABLE_GETTER) {
        retExpr = CreateRefExpr(fieldDecl);
    } else {
        auto outerClassDecl = As<ASTKind::CLASS_DECL>(fieldDecl.outerDecl);
        CJC_ASSERT(outerClassDecl);
        retExpr = CreateMemberAccess(
            kind == AccessorKind::STATIC_FIELD_GETTER ?
                CreateRefExpr(*outerClassDecl) : CreateThisRef(*outerClassDecl),
            fieldDecl.identifier);
    }

    std::vector<OwnedPtr<Node>> bodyNodes;
    bodyNodes.emplace_back(CreateReturnExpr(std::move(retExpr), &funcBody));
    return bodyNodes;
}

std::vector<OwnedPtr<Node>> MockSupportManager::GenerateFieldSetterAccessorBody(
    VarDecl& fieldDecl, FuncParam& setterParam, FuncBody& funcBody, AccessorKind kind) const
{
    static const auto UNIT_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    OwnedPtr<Expr> retExpr;

    if (kind == AccessorKind::TOP_LEVEL_VARIABLE_SETTER) {
        retExpr = CreateRefExpr(fieldDecl);
    } else {
        auto outerClassDecl = As<ASTKind::CLASS_DECL>(fieldDecl.outerDecl);
        CJC_ASSERT(outerClassDecl);
        auto refExpr = kind == AccessorKind::STATIC_FIELD_SETTER ?
            CreateRefExpr(*outerClassDecl) : CreateThisRef(*outerClassDecl);
        retExpr = CreateMemberAccess(std::move(refExpr), fieldDecl.identifier);
    }

    std::vector<OwnedPtr<Node>> bodyNodes;
    bodyNodes.emplace_back(
        CreateReturnExpr(CreateAssignExpr(std::move(retExpr), CreateRefExpr(setterParam), UNIT_TY), &funcBody));
    return bodyNodes;
}

OwnedPtr<FuncDecl> MockSupportManager::CreateFieldAccessorDecl(
    const VarDecl &fieldDecl, FuncTy *accessorTy, AccessorKind kind) const
{
    OwnedPtr<FuncDecl> accessorDecl = MakeOwned<FuncDecl>();

    accessorDecl->curFile = fieldDecl.curFile;
    accessorDecl->keywordPos = fieldDecl.keywordPos;
    accessorDecl->identifier.SetPos(fieldDecl.identifier.Begin(), fieldDecl.identifier.End());
    accessorDecl->moduleName = fieldDecl.moduleName;
    accessorDecl->fullPackageName = fieldDecl.fullPackageName;
    accessorDecl->outerDecl = As<ASTKind::CLASS_DECL>(fieldDecl.outerDecl);
    accessorDecl->ty = accessorTy;
    accessorDecl->identifier = MockUtils::BuildMockAccessorIdentifier(fieldDecl, kind);
    MarkMockAccessorWithAttributes(*accessorDecl);

    return accessorDecl;
}

OwnedPtr<FuncDecl> MockSupportManager::CreateForeignFunctionAccessorDecl(FuncDecl& funcDecl) const
{
    static const auto NOTHING_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_NOTHING);

    CJC_ASSERT(funcDecl.TestAttr(Attribute::FOREIGN));
    const auto& funcBody = funcDecl.funcBody;

    CJC_ASSERT(funcDecl.ty->kind == TypeKind::TYPE_FUNC);
    auto funcTy = Ptr(StaticCast<FuncTy>(funcDecl.ty));

    std::vector<OwnedPtr<FuncParamList>> accessorFuncParamLists;
    for (const auto& paramList : funcBody->paramLists) {
        std::vector<OwnedPtr<FuncParam>> accessorParamList;
        for (const auto& param : paramList->params) {
            auto paramDecl = As<ASTKind::FUNC_PARAM>(param);
            CJC_ASSERT(paramDecl);
            auto accessorParamDecl = CreateFuncParam(
                paramDecl->identifier,
                ASTCloner::Clone(paramDecl->type.get()),
                nullptr,
                paramDecl->ty);
            accessorParamList.emplace_back(std::move(accessorParamDecl));
        }
        accessorFuncParamLists.emplace_back(
            CreateFuncParamList(std::move(accessorParamList)));
    }

    std::vector<OwnedPtr<FuncArg>> args;
    for (const auto& paramList : accessorFuncParamLists) {
        for (const auto& param : paramList->params) {
            args.emplace_back(CreateFuncArg(CreateRefExpr(*param)));
        }
    }

    auto accessorFuncRetStmt = CreateReturnExpr(
        CreateCallExpr(CreateRefExpr(funcDecl), std::move(args), nullptr, funcTy->retTy));
    accessorFuncRetStmt->ty = NOTHING_TY;
    std::vector<OwnedPtr<Node>> accessorFuncBodyStmts;
    accessorFuncBodyStmts.emplace_back(std::move(accessorFuncRetStmt));
    auto accessorFuncBodyBlock = CreateBlock(std::move(accessorFuncBodyStmts), NOTHING_TY);

    auto accessorFuncBody = CreateFuncBody(
        std::move(accessorFuncParamLists),
        ASTCloner::Clone(funcBody->retType.get()),
        std::move(accessorFuncBodyBlock),
        funcTy);

    auto accessorName = MockUtils::GetForeignAccessorName(funcDecl) + MockUtils::mockAccessorSuffix;
    auto accessorDecl = CreateFuncDecl(accessorName,  std::move(accessorFuncBody), funcTy);
    accessorDecl->curFile = funcDecl.curFile;
    accessorDecl->fullPackageName = funcDecl.fullPackageName;
    accessorDecl->moduleName = funcDecl.moduleName;
    accessorDecl->EnableAttr(Attribute::PUBLIC);
    accessorDecl->EnableAttr(Attribute::GLOBAL);
    accessorDecl->EnableAttr(Attribute::UNSAFE);
    accessorDecl->EnableAttr(Attribute::GENERATED_TO_MOCK);
    accessorDecl->EnableAttr(Attribute::NO_MANGLE);

    return accessorDecl;
}

OwnedPtr<FuncDecl> MockSupportManager::GenerateVarDeclAccessor(VarDecl& fieldDecl, AccessorKind kind)
{
    CJC_ASSERT(IsFieldOrVariable(kind));

    auto isGetter = mockUtils->IsGeneratedGetter(kind);

    FuncTy* accessorTy = isGetter ?
        typeManager.GetFunctionTy({}, fieldDecl.ty) :
        typeManager.GetFunctionTy({fieldDecl.ty}, TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
    std::vector<OwnedPtr<Node>> body;
    std::vector<OwnedPtr<FuncParam>> accessorParams {};

    OwnedPtr<Type> fieldType =
        fieldDecl.type ? ASTCloner::Clone(fieldDecl.type.get()) : MockUtils::CreateType<RefType>(fieldDecl.ty);

    auto accessorDecl = CreateFieldAccessorDecl(fieldDecl, accessorTy, kind);
    accessorDecl->funcBody = MakeOwned<FuncBody>();

    if (isGetter) {
        body = GenerateFieldGetterAccessorBody(fieldDecl, *accessorDecl->funcBody, kind);
    } else {
        auto setterParam = CreateFuncParam("newValue", std::move(fieldType), nullptr, fieldDecl.ty);
        setterParam->outerDecl = accessorDecl.get();
        setterParam->moduleName = fieldDecl.moduleName;
        setterParam->fullPackageName = fieldDecl.fullPackageName;
        setterParam->curFile = fieldDecl.curFile;
        body = GenerateFieldSetterAccessorBody(fieldDecl, *setterParam, *accessorDecl->funcBody, kind);
        accessorParams.emplace_back(std::move(setterParam));
    }

    std::vector<OwnedPtr<FuncParamList>> accessorParamLists {};
    accessorParamLists.emplace_back(CreateFuncParamList(std::move(accessorParams)));

    OwnedPtr<Type> retType = isGetter ?
        std::move(fieldType) : MockUtils::CreateType<PrimitiveType>(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));

    if (IsStaticField(kind)) {
        accessorDecl->EnableAttr(Attribute::STATIC);
    }
    if (IsTopLevelField(kind)) {
        accessorDecl->EnableAttr(Attribute::GLOBAL);
    }
    accessorDecl->funcBody->ty = accessorTy;
    accessorDecl->funcBody->parentClassLike = As<ASTKind::CLASS_DECL>(fieldDecl.outerDecl);
    accessorDecl->funcBody->funcDecl = accessorDecl.get();
    accessorDecl->funcBody->paramLists = std::move(accessorParamLists);
    accessorDecl->funcBody->body = CreateBlock(std::move(body), accessorTy);
    accessorDecl->funcBody->ty = accessorTy;
    accessorDecl->funcBody->retType = std::move(retType);
    accessorDecl->mangledName = mockUtils->mangler.Mangle(*accessorDecl);

    if (fieldDecl.ty->IsStruct() && !IsStaticField(kind) && !IsTopLevelField(kind)) {
        accessorDecl->EnableAttr(Attribute::MUT);
    }

    return accessorDecl;
}

bool MockSupportManager::NeedToSearchCallsToReplaceWithAccessors(Node& node)
{
    // Accessors only contain a call of their original declaration which shouldn't be replaced
    if (auto decl = As<ASTKind::DECL>(&node); decl && MockUtils::IsMockAccessor(*decl)) {
        return false;
    }

    return true;
}

void MockSupportManager::WriteGeneratedMockDecls()
{
    while (!generatedMockDecls.empty()) {
        auto accessorDecl = std::move(generatedMockDecls.extract(generatedMockDecls.begin()).value());
        mockUtils->instantiate(*accessorDecl);
        if (auto outerDecl = As<ASTKind::CLASS_DECL>(accessorDecl->outerDecl); outerDecl) {
            outerDecl->body->decls.emplace_back(std::move(accessorDecl));
        } else if (accessorDecl->curFile) {
            auto file = accessorDecl->curFile;
            file->decls.emplace_back(std::move(accessorDecl));
            std::rotate(file->decls.rbegin(), file->decls.rbegin() + 1, file->decls.rend());
        }
    }
    generatedMockDecls.clear();
    genericMockVarsDecls.clear();
}

bool MockSupportManager::IsMemberAccessOnThis(const MemberAccess& memberAccess) const
{
    if (!memberAccess.baseExpr) {
        return false;
    }

    auto refBaseExpr = As<ASTKind::REF_EXPR>(memberAccess.baseExpr);

    if (!refBaseExpr) {
        return false;
    }

    return refBaseExpr->isThis;
}

/*
 * For calls involving mut operations on structs,
 * we cannot just replace intermediate member access expression (like field access) with an accessor call,
 * because it causes mutability rules violation.
 * Foe example, `foo.myStructField.mutY(newY)` -> `foo.myStructField$get().mutY(newY)`  <--- this is wrong replacement
 * Instead, we extract that intermediate expression into a mutable variable,
 * then substitute it instead of original expression.
 * And, finally invoke a setter accessor to pass the mutated struct back.
 *
 * Example of desugaring:
 *  myClass.myStruct.mutSomeField()
 *      =>
 *  {
 *      var $tmp1 = myClass.myStruct$get$ToMock()
 *      let $tmp2 = $tmp1.mutSomeField()
 *      myClass.myStruct$set$ToMock($tmp1)
 *      $tmp2
 *  }
 */
void MockSupportManager::TransformAccessorCallForMutOperation(
    MemberAccess& originalMa, Expr& replacedMa, Expr& topLevelExpr)
{
    CJC_ASSERT(Is<AssignExpr>(topLevelExpr) ||
        (Is<CallExpr>(topLevelExpr) &&
            DynamicCast<CallExpr*>(&topLevelExpr)->resolvedFunction->TestAttr(Attribute::MUT)));

    auto tmpVarDecl = CreateTmpVarDecl(
        MockUtils::CreateType<Type>(replacedMa.ty),
        ASTCloner::Clone(Ptr(&replacedMa)));
    tmpVarDecl->isVar = true;

    auto tmpVarRefToMutate = CreateRefExpr(*tmpVarDecl);
    tmpVarRefToMutate->ref.identifier = tmpVarDecl->identifier;
    tmpVarRefToMutate->curFile = replacedMa.curFile;
    tmpVarRefToMutate->ty = originalMa.ty;

    OwnedPtr<Expr> newTopLevelExpr = ASTCloner::Clone(Ptr(&topLevelExpr));
    Ptr<Expr> mutBaseExpr;
    if (auto callExpr = As<ASTKind::CALL_EXPR>(newTopLevelExpr)) {
        mutBaseExpr = callExpr->baseFunc;
    } else if (auto assignExpr = As<ASTKind::ASSIGN_EXPR>(newTopLevelExpr)) {
        mutBaseExpr = assignExpr->leftValue;
    } else {
        CJC_ABORT();
    }
    auto tmpVarRefToAssign = ASTCloner::Clone(tmpVarRefToMutate.get());
    CJC_ASSERT(mutBaseExpr->astKind == ASTKind::MEMBER_ACCESS);
    As<ASTKind::MEMBER_ACCESS>(mutBaseExpr)->baseExpr = std::move(tmpVarRefToMutate);

    auto mutResultVarDecl = CreateTmpVarDecl(
        MockUtils::CreateType<RefType>(newTopLevelExpr->ty), std::move(newTopLevelExpr));
    auto mutResultVarRef = CreateRefExpr(*mutResultVarDecl);
    mutResultVarRef->ref.identifier = mutResultVarDecl->identifier;
    mutResultVarRef->curFile = replacedMa.curFile;

    auto ty = topLevelExpr.ty;

    auto newOriginalMa = ASTCloner::Clone(Ptr(&originalMa));
    newOriginalMa->desugarExpr = nullptr;

    auto backAssignExpr = CreateAssignExpr(std::move(newOriginalMa), std::move(tmpVarRefToAssign));
    backAssignExpr->ty = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    ReplaceFieldSetWithAccessor(*backAssignExpr, false);
    backAssignExpr->EnableAttr(Attribute::GENERATED_TO_MOCK);

    std::vector<OwnedPtr<Node>> nodes {};
    nodes.emplace_back(std::move(tmpVarDecl));
    nodes.emplace_back(std::move(mutResultVarDecl));
    nodes.emplace_back(std::move(backAssignExpr));
    nodes.emplace_back(CreateReturnExpr(std::move(mutResultVarRef), nullptr));

    std::vector<OwnedPtr<FuncParamList>> paramLists {};
    paramLists.emplace_back(CreateFuncParamList(std::vector<Ptr<FuncParam>> {}));

    auto lambda = CreateLambdaExpr(
        CreateFuncBody(
            std::move(paramLists),
            MockUtils::CreateType<Type>(replacedMa.ty), CreateBlock(std::move(nodes), ty), ty)
    );
    lambda->ty = typeManager.GetFunctionTy({}, ty);
    lambda->funcBody->ty = lambda->ty;

    topLevelExpr.desugarExpr = CreateCallExpr(std::move(lambda), {}, nullptr, ty);
}

void MockSupportManager::ReplaceSubMemberAccessWithAccessor(
    const MemberAccess& memberAccess, bool isInConstructor, const Ptr<Expr> topLevelMutExpr)
{
    if (auto subMa = As<ASTKind::MEMBER_ACCESS>(ExtractLastDesugaredExpr(*memberAccess.baseExpr)); subMa) {
        auto replacedSubMa = ReplaceExprWithAccessor(*subMa, isInConstructor, true);
        if (topLevelMutExpr && replacedSubMa) {
            TransformAccessorCallForMutOperation(*subMa, *replacedSubMa, *topLevelMutExpr);
        }
    }
}

Ptr<Expr> MockSupportManager::ReplaceExprWithAccessor(Expr& originalExpr, bool isInConstructor, bool isSubMemberAccess)
{
    auto expr = ExtractLastDesugaredExpr(originalExpr);
    if (auto fieldMemberAccess = As<ASTKind::MEMBER_ACCESS>(expr);
        fieldMemberAccess && fieldMemberAccess->target &&
        fieldMemberAccess->target->astKind == ASTKind::VAR_DECL &&
        fieldMemberAccess->target->astKind != ASTKind::PROP_DECL
    ) {
        // Left values of an assignment are handled below, by `ReplaceFieldSetWithAccessor`
        if (fieldMemberAccess->TestAttr(Attribute::LEFT_VALUE) || (!fieldMemberAccess->isAlone && !isSubMemberAccess)) {
            return nullptr;
        }
        return ReplaceFieldGetWithAccessor(*fieldMemberAccess, isInConstructor);
    } else if (auto assignment = As<ASTKind::ASSIGN_EXPR>(expr); assignment &&
        !assignment->TestAttr(Attribute::GENERATED_TO_MOCK)
    ) {
        // Some left value expressions don't have `LEFT_VALUE` attribute,
        // set the attribute to definitely skip left value expressions themthelves to generate accessor calls
        // as they are handled within whole assign expressions
        assignment->leftValue->EnableAttr(Attribute::LEFT_VALUE);

        // Support all compound assignments
        if (!assignment->isCompound) {
            return ReplaceFieldSetWithAccessor(*assignment, isInConstructor);
        } else {
            return nullptr;
        }
    } else if (auto memberAccess = As<ASTKind::MEMBER_ACCESS>(expr);  memberAccess && memberAccess->target) {
        return ReplaceMemberAccessWithAccessor(*memberAccess, isInConstructor);
    } else if (auto refExpr = As<ASTKind::REF_EXPR>(expr);
        refExpr && refExpr->GetTarget() && !refExpr->TestAttr(Attribute::LEFT_VALUE)
    ) {
        auto target = refExpr->GetTarget();
        if (target->astKind == ASTKind::VAR_DECL && target->TestAttr(Attribute::GLOBAL)) {
            return ReplaceTopLevelVariableGetWithAccessor(*refExpr);
        }
        return nullptr;
    } else {
        return nullptr;
    }
}

Ptr<Expr> MockSupportManager::ReplaceMemberAccessWithAccessor(MemberAccess& memberAccess, bool isInConstructor)
{
    Ptr<Expr> parentMutExpr = nullptr;
    if (auto ce = DynamicCast<CallExpr*>(memberAccess.callOrPattern)) {
        if (auto resolvedFunction = ce->resolvedFunction; resolvedFunction &&
            resolvedFunction->TestAttr(Attribute::MUT) && Is<StructDecl>(resolvedFunction->outerDecl)) {
            parentMutExpr = ce;
        }
    }
    ReplaceSubMemberAccessWithAccessor(memberAccess, isInConstructor, parentMutExpr);

    if (isInConstructor && IsMemberAccessOnThis(memberAccess)) {
        return nullptr;
    }

    if (auto funcDecl = As<ASTKind::FUNC_DECL>(memberAccess.target); funcDecl && funcDecl->propDecl) {
        auto propDeclToMock = As<ASTKind::PROP_DECL>(
            mockUtils->FindAccessorForMemberAccess(memberAccess, funcDecl->propDecl, {}, AccessorKind::METHOD));
        if (!propDeclToMock) {
            return nullptr;
        }
        if (funcDecl->isGetter) {
            memberAccess.target = GetUsableGetterForProperty(*propDeclToMock);
        } else if (funcDecl->isSetter) {
            memberAccess.target = GetUsableSetterForProperty(*propDeclToMock);
        }
    } else if (auto funcDeclToMock = mockUtils->FindAccessorForMemberAccess(
        memberAccess, memberAccess.target, memberAccess.instTys, AccessorKind::METHOD)) {
        memberAccess.target = funcDeclToMock;
    }

    // No desugared expr generated here (instead, target is replaced) so return original member access
    return &memberAccess;
}

Ptr<Expr> MockSupportManager::ReplaceFieldGetWithAccessor(MemberAccess& memberAccess, bool isInConstructor)
{
    ReplaceSubMemberAccessWithAccessor(memberAccess, isInConstructor);

    if (!memberAccess.target || (isInConstructor && IsMemberAccessOnThis(memberAccess))) {
        return nullptr;
    }

    if (auto accessorCall = GenerateAccessorCallForField(memberAccess, AccessorKind::FIELD_GETTER); accessorCall) {
        accessorCall->sourceExpr = Ptr(&memberAccess);
        memberAccess.desugarExpr = std::move(accessorCall);
        return memberAccess.desugarExpr;
    }

    return nullptr;
}

Ptr<Expr> MockSupportManager::ReplaceTopLevelVariableGetWithAccessor(RefExpr& refExpr)
{
    if (!refExpr.GetTarget()) {
        return nullptr;
    }

    if (auto accessorCall = GenerateAccessorCallForTopLevelVariable(
        refExpr, AccessorKind::TOP_LEVEL_VARIABLE_GETTER); accessorCall
    ) {
        accessorCall->sourceExpr = Ptr(&refExpr);
        refExpr.desugarExpr = std::move(accessorCall);
        return refExpr.desugarExpr;
    }

    return nullptr;
}

Ptr<Expr> MockSupportManager::ReplaceFieldSetWithAccessor(AssignExpr& assignExpr, bool isInConstructor)
{
    auto leftValue = assignExpr.leftValue.get();
    OwnedPtr<CallExpr> accessorCall;
    if (auto refExpr = As<ASTKind::REF_EXPR>(leftValue); refExpr) {
        if (!refExpr->GetTarget()) {
            return nullptr;
        }
        accessorCall = GenerateAccessorCallForTopLevelVariable(*refExpr, AccessorKind::TOP_LEVEL_VARIABLE_SETTER);
        if (!accessorCall) {
            return nullptr;
        }
    } else if (auto memberAccess = As<ASTKind::MEMBER_ACCESS>(leftValue); memberAccess) {
        if (!memberAccess->target || (isInConstructor && IsMemberAccessOnThis(*memberAccess))) {
            return nullptr;
        }
        accessorCall = GenerateAccessorCallForField(*memberAccess, AccessorKind::FIELD_SETTER);
        if (!accessorCall) {
            ReplaceSubMemberAccessWithAccessor(
                *memberAccess, isInConstructor,
                Is<StructDecl>(memberAccess->target->outerDecl) ? &assignExpr : nullptr);
            return &assignExpr;
        }
    }

    accessorCall->args.emplace_back(CreateFuncArg(ASTCloner::Clone(assignExpr.rightExpr.get())));
    accessorCall->sourceExpr = Ptr(&assignExpr);
    assignExpr.desugarExpr = std::move(accessorCall);
    return assignExpr.desugarExpr;
}

OwnedPtr<CallExpr> MockSupportManager::GenerateAccessorCallForTopLevelVariable(
    const RefExpr& refExpr, AccessorKind kind)
{
    auto accessorDecl = mockUtils->FindTopLevelAccessor(refExpr.GetTarget(), kind);
    if (!accessorDecl) {
        return nullptr;
    }

    auto accessorCall = MakeOwned<CallExpr>();

    accessorCall->ty = RawStaticCast<const FuncTy*>(accessorDecl->ty)->retTy;
    accessorCall->resolvedFunction = accessorDecl;
    accessorCall->baseFunc = CreateRefExpr(*accessorDecl);
    std::vector<OwnedPtr<FuncArg>> mockedMethodArgRefs {};
    accessorCall->args = std::move(mockedMethodArgRefs);
    accessorCall->callKind = CallKind::CALL_DECLARED_FUNCTION;
    accessorCall->curFile = refExpr.curFile;

    return accessorCall;
}

OwnedPtr<CallExpr> MockSupportManager::GenerateAccessorCallForField(
    const MemberAccess& memberAccess, AccessorKind kind)
{
    Ptr<FuncDecl> accessorDecl = As<ASTKind::FUNC_DECL>(
        mockUtils->FindAccessorForMemberAccess(memberAccess, memberAccess.target, memberAccess.instTys, kind));

    if (!accessorDecl) {
        return nullptr;
    }

    auto maTy = memberAccess.ty;
    auto accessorCall = MakeOwned<CallExpr>();
    auto accessorMemberAccess = CreateMemberAccess(
        ASTCloner::Clone(memberAccess.baseExpr.get()), accessorDecl->identifier);

    accessorMemberAccess->curFile = memberAccess.curFile;
    accessorMemberAccess->target = accessorDecl;
    accessorMemberAccess->callOrPattern = accessorCall.get();

    if (kind == AccessorKind::FIELD_GETTER) {
        accessorMemberAccess->ty = typeManager.GetFunctionTy({}, maTy);
        accessorCall->ty = maTy;
    } else {
        accessorMemberAccess->ty = typeManager.GetFunctionTy({maTy}, TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
        accessorCall->ty = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    }

    accessorCall->resolvedFunction = accessorDecl;
    accessorCall->baseFunc = std::move(accessorMemberAccess);
    std::vector<OwnedPtr<FuncArg>> mockedMethodArgRefs {};
    accessorCall->args = std::move(mockedMethodArgRefs);
    accessorCall->callKind = CallKind::CALL_DECLARED_FUNCTION;
    accessorCall->curFile = memberAccess.curFile;

    return accessorCall;
}

}
