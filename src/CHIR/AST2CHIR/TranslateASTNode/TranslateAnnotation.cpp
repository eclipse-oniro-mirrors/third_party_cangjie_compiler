// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/Expression.h"
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
#include "cangjie/Mangle/CHIRManglingUtils.h"
#endif

using namespace Cangjie::CHIR;
using namespace Cangjie;

static const std::string& GetIdentifierToPrint(const AST::Decl& decl)
{
    if (auto prop = DynamicCast<AST::PropDecl>(&decl)) {
        if (!prop->getters.empty()) {
            return prop->getters[0]->identifier;
        }
        if (!prop->setters.empty()) {
            return prop->setters[0]->identifier;
        }
    }
    return decl.identifier;
}

void Translator::TranslateAnnoFactoryFuncBody(const AST::Decl& decl, Func& func)
{
    auto body = builder.CreateBlockGroup(func);
    blockGroupStack.emplace_back(body);
    func.InitBody(*body);
    func.EnableAttr(Attribute::COMPILER_ADD);
    // create body
    auto bodyBlock = CreateBlock();
    currentBlock = bodyBlock;
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    auto res = TranslateExprArg(*decl.annotationsArray);
#endif
    auto entry = builder.CreateBlock(body);
    body->SetEntryBlock(entry);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    auto retType = res->GetType();
    auto expr = CreateAndAppendExpression<Allocate>(DebugLocation(), builder.GetType<RefType>(retType), retType, entry);
    auto retAlloc = expr->GetResult();

    func.SetReturnValue(*retAlloc);
    // create exit
    CreateWrappedStore(res, retAlloc, currentBlock);
#endif
    CreateAndAppendTerminator<Exit>(currentBlock);
    CreateAndAppendTerminator<GoTo>(bodyBlock, entry);
    blockGroupStack.pop_back();
}

AnnoInfo Translator::CreateAnnoFactoryFuncs(const AST::Decl& decl, CustomTypeDef* parent)
{
    auto annosArray = decl.annotationsArray.get();
    if (decl.TestAttr(AST::Attribute::IMPORTED) || !annosArray || annosArray->children.empty()) {
        return {"none"};
    }
    auto found = annotationFuncMap.find(&decl);
    if (found != annotationFuncMap.end()) {
        return {found->second}; // Property's getters and setters share the same annotation function.
    }
    Type* returnTy = nullptr;
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    returnTy = TranslateType(*annosArray->ty);
#endif
    auto funcType = builder.GetType<FuncType>(std::vector<Type*>{}, returnTy);
    const auto& loc = TranslateLocation(*decl.annotationsArray->children[0]);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    std::string mangledName = CHIRMangling::GenerateAnnotationFuncMangleName(decl.mangledName);
#endif
    if (opts.chirDebugOptimizer) {
        std::string ms = "The annotation factory function of " + GetIdentifierToPrint(decl) + " in the line " +
            std::to_string(decl.begin.line) + " is " + mangledName + '\n';
        std::cout << ms;
    }
    auto func = builder.CreateFunc(loc, funcType, mangledName, mangledName, "", decl.fullPackageName);
    func->SetFuncKind(FuncKind::ANNOFACTORY_FUNC);
    annoFactoryFuncs.emplace_back(&decl, func);
    annotationFuncMap.emplace(&decl, mangledName);
    if (parent) {
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        parent->AddMethod(func);
#endif
    }
    func->EnableAttr(Attribute::STATIC);

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    // Collect annotations whose parameter values are literal constants.
    std::vector<AnnoInfo::AnnoPair> annoPairs;
    for (auto& elem : annosArray->children) {
        auto callExpr = StaticCast<AST::CallExpr*>(elem.get().get());
        auto& callee = callExpr->resolvedFunction->funcBody;
        auto annoClassDecl = callee->parentClassLike;
        CJC_ASSERT(annoClassDecl);
        auto annoClassDeclName = annoClassDecl->identifier.GetRawText();
        std::vector<std::string> paramValues;
        for (auto& arg : callExpr->args) {
            if (auto& argVal = arg->expr; argVal->astKind == AST::ASTKind::LIT_CONST_EXPR) {
                auto lit = StaticCast<AST::LitConstExpr*>(argVal.get().get());
                paramValues.emplace_back(lit->rawString);
            } else {
                return {mangledName, {}};
            }
        }
        annoPairs.emplace_back(annoClassDeclName, paramValues);
    }
    return {mangledName, annoPairs};
#endif
}

std::unordered_map<std::string, Ptr<Func>> Translator::jAnnoFuncMap;

void Translator::CreateParamAnnotationInfo(const AST::FuncParam& astParam, Parameter& chirParam, CustomTypeDef& parent)
{
    chirParam.SetAnnoInfo(CreateAnnoFactoryFuncs(astParam, &parent));
}

void Translator::CreateAnnoFactoryFuncsForFuncDecl(const AST::FuncDecl& funcDecl, CustomTypeDef* parent)
{
    auto& params = funcDecl.funcBody->paramLists[0]->params;
    auto funcValue = GetSymbolTable(funcDecl);
    const AST::Decl& annotatedDecl = funcDecl.propDecl ? *funcDecl.propDecl : StaticCast<AST::Decl>(funcDecl);
    if (auto func = DynamicCast<Func>(funcValue)) {
        CreateAnnotationInfo<Func>(annotatedDecl, *func, *parent);
        size_t offset = params.size() == func->GetNumOfParams() ? 0 : 1;
        for (size_t i = 0; i < params.size(); ++i) {
            CreateParamAnnotationInfo(*params[i], *func->GetParam(i + offset), *parent);
        }
    } else if (!funcDecl.TestAttr(AST::Attribute::IMPORTED) && funcValue->TestAttr(Attribute::NON_RECOMPILE)) {
        // Update annotation info for incremental created 'ImportedValue';
        auto importedFunc = DynamicCast<ImportedFunc>(funcValue);
        CJC_NULLPTR_CHECK(importedFunc);
        CreateAnnotationInfo<ImportedFunc>(annotatedDecl, *importedFunc, *parent);
        auto paramInfo = importedFunc->GetParamInfo();
        for (size_t i = 0; i < params.size(); ++i) {
            paramInfo[i].annoInfo = CreateAnnoFactoryFuncs(*params[i], parent);
        }
        importedFunc->SetParamInfo(std::move(paramInfo));
    }
}
