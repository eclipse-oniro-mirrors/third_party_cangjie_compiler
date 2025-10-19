// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/AST/Utils.h"
#include "cangjie/CHIR/AST2CHIR/AST2CHIR.h"

#include <fstream>

#include "cangjie/CHIR/AST2CHIR/GenerateVTable/UpdateOperatorVTable.h"
#include "cangjie/CHIR/AST2CHIR/GenerateVTable/VTableGenerator.h"
#include "cangjie/CHIR/AST2CHIR/GenerateVTable/WrapVirtualFunc.h"
#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/AST2CHIR/Utils.h"
#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/ConstantUtils.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::CHIR {
namespace {
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, init, SpawnException);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, init, Exception);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, init, IndexOutOfBoundsException);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, init, NegativeArraySizeException);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, init, OutOfMemoryError);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, printStackTrace, Exception);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, printStackTrace, Error);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, getCommandLineArgs);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, createArithmeticExceptionMsg);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, createOverflowExceptionMsg);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, CJ_CORE_ExecAtexitCallbacks);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, handleException);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::INVALID_DECL, eprintln);
REG_IMPLICIT_IMPORTED_NON_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, setRuntimeCJThreadHandle, Thread);

REG_IMPLICIT_IMPORTED_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, execute, Future);
REG_IMPLICIT_IMPORTED_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, executeClosure, Future);
REG_IMPLICIT_IMPORTED_GENERIC_FUNC(AST::ASTKind::CLASS_DECL, get, Future);

Linkage GetFuncLinkage(const GlobalOptions& opts, const AST::FuncDecl& func)
{
    if (func.TestAttr(AST::Attribute::IMPORTED)) {
        return func.linkage;
    }
    if (func.linkage == Linkage::EXTERNAL || func.linkage == Linkage::WEAK_ODR) {
        if (func.TestAttr(AST::Attribute::IMPORTED)) {
            return Linkage::INTERNAL;
        } else if (opts.target.IsMinGW()) {
            return Linkage::EXTERNAL;
        }
    }
    return func.linkage;
}

bool IsLocalConstFuncInStaticMember(const AST::FuncDecl& astFunc)
{
    if (!astFunc.isConst) {
        return false;
    }
    return IsLocalFunc(astFunc) && GetOuterDecl(astFunc)->TestAttr(AST::Attribute::STATIC);
}

void SetGlobalVarLinkageType(Value& var, const AST::VarDecl& decl, bool isLiftedLocalConst, bool isWin)
{
    // local const var is lifted to global const var by CHIR, its linkage must be internal
    if (isLiftedLocalConst) {
        var.Set<LinkTypeInfo>(Linkage::INTERNAL);
        return;
    }
    if (decl.isConst && !isWin) {
        var.Set<LinkTypeInfo>(Linkage::WEAK_ODR);
        return;
    }
    var.Set<LinkTypeInfo>(decl.linkage);
}
} // namespace

void AST2CHIR::AddToImplicitFuncs(AST::FuncDecl& funcDecl,
    std::vector<ImplicitImportedFunc>& registeredImplicitFuncs,
    std::unordered_set<Ptr<const AST::Decl>>& implicitlyImportedDecls) const
{
    std::unordered_set<std::string> usedFuncInSanCov{FUNC_MANGLE_NAME_MALLOC_CSTRING, FUNC_MANGLE_NAME_CSTRING_SIZE};
    if (opts.sancovOption.IsSancovEnabled() && usedFuncInSanCov.find(funcDecl.mangledName) != usedFuncInSanCov.end()) {
        implicitlyImportedDecls.emplace(&funcDecl);
        return;
    }
    auto checkFuncInfo = [](const AST::FuncDecl& funcDecl, const ImplicitImportedFunc& funcInfo) {
        const auto parentDecl = funcDecl.outerDecl;
        return funcDecl.identifier == funcInfo.identifier &&
            (funcInfo.parentName == "" ? !parentDecl
                                       : (parentDecl && parentDecl->identifier == funcInfo.parentName &&
                                             parentDecl->astKind == funcInfo.parentKind));
    };
    // collect implicit funcDecl.
    auto it = std::find_if(registeredImplicitFuncs.begin(), registeredImplicitFuncs.end(),
        [&checkFuncInfo, &funcDecl](auto& funcInfo) { return checkFuncInfo(funcDecl, funcInfo); });
    if (it != registeredImplicitFuncs.end()) {
        implicitlyImportedDecls.emplace(&funcDecl);
    }
}

void AST2CHIR::CollectImplicitFuncs()
{
    std::vector<ImplicitImportedFunc> registeredImplicitFuncs{};

    auto collectImplicitDecls = [&registeredImplicitFuncs, this](AST::Node* node) -> AST::VisitAction {
        if (node->astKind == AST::ASTKind::FUNC_DECL) {
            auto funcDecl = StaticCast<AST::FuncDecl*>(node);
            AddToImplicitFuncs(*funcDecl, registeredImplicitFuncs, implicitDecls);
            return AST::VisitAction::SKIP_CHILDREN;
        }
        return AST::VisitAction::WALK_CHILDREN;
    };

    auto importedPkgs = importManager.GetAllImportedPackages();
    AST::PackageDecl* stdCorePkg = importManager.GetPackageDecl(CORE_PACKAGE_NAME);
    CJC_NULLPTR_CHECK(stdCorePkg);
    // Collect implicitly imported/used generic funcDecl.
    // These generic functions that are called implicitly only in CodeGen are from the "std.core" package.
    // But their generic instances may be in other import packages,so all import packages need to be traversed.
    registeredImplicitFuncs = ImplicitImportedFuncMgr::Instance().GetImplicitImportedFuncs(
        ImplicitImportedFuncMgr::FuncKind::GENERIC);
    for (auto& importedPkg : importedPkgs) {
        for (auto& instantiatedDecl : importedPkg->srcPackage->genericInstantiatedDecls) {
            AST::Walker(instantiatedDecl.get(), collectImplicitDecls).Walk();
        }
    }
    // Collect implicitly imported/used non-generic funcDecl.
    // These functions that are called implicitly only in CodeGen are from the "std.core" package.
    registeredImplicitFuncs = ImplicitImportedFuncMgr::Instance().GetImplicitImportedFuncs(
        ImplicitImportedFuncMgr::FuncKind::NONE_GENERIC);
    AST::IterateToplevelDecls(*stdCorePkg->srcPackage, [&collectImplicitDecls](const OwnedPtr<AST::Decl>& decl) {
        AST::Walker(decl.get(), collectImplicitDecls).Walk();
    });
    for (auto& implicitDecl : implicitDecls) {
        CJC_ASSERT(implicitDecl->IsFunc());
    }
}

void AST2CHIR::CollectDeclToList(AST::Decl& decl, std::vector<Ptr<const AST::Decl>>& astNodes)
{
    allTopLevelNodes.emplace_back(&decl);
    astNodes.emplace_back(&decl);
}

void AST2CHIR::CollectDeclsFromEnumDecl(AST::EnumDecl& enumDecl)
{
    for (auto& member : enumDecl.members) {
        // enumDecl's members include: member function, override function, prop function, associated type
        CJC_ASSERT(Is<AST::PropDecl*>(member.get()) || Is<AST::FuncDecl*>(member.get()));
        CollectMemberDecl(*member);
    }
}

void AST2CHIR::CollectDeclsFromExtendDecl(AST::ExtendDecl& extendDecl)
{
    // include member function, static member function, prop func, static prop func.
    for (auto& member : extendDecl.members) {
        CollectMemberDecl(*member);
    }
}

void AST2CHIR::CollectDeclsFromClassLikeDecl(AST::ClassLikeDecl& classLikeDecl)
{

    auto& bodyDecls = classLikeDecl.GetMemberDecls();
    for (auto& member : bodyDecls) {
        // Skip the non-static member variable declaration
        if (member->astKind == AST::ASTKind::VAR_DECL && !member.get()->TestAttr(AST::Attribute::STATIC)) {
            continue;
        }

        CollectMemberDecl(*member);
    }
}

void AST2CHIR::CollectInstantiatedDecls(const AST::Decl& decl)
{
    /**
     * This function should only collect instantiated decls whose generic decl is in current package.
     * if the generic decl is in upstream package, but instantiated decls is in current package,
     * which should be collected by `CollectImportedGenericInstantiatedDecl`
     */
    CJC_ASSERT(decl.TestAttr(AST::Attribute::GENERIC));
    if (decl.curFile->curPackage->fullPackageName != GetPackage()->GetName()) {
        return;
    }
    if (!gim) {
        return;
    }
    auto instantiatedDecls = gim->GetInstantiatedDecls(decl);
    // For bep, decl should be sorted by mangledName and position to make sequence stable.
    auto compare = [](const AST::Decl* d1, const AST::Decl* d2) {
        CJC_NULLPTR_CHECK(d1);
        CJC_NULLPTR_CHECK(d2);
        CJC_ASSERT(d1->mangledName != d2->mangledName);
        return d1->mangledName < d2->mangledName;
    };
    std::set<AST::Decl*, decltype(compare)> sortedInstantiatedDecls(compare);
    for (auto& tempDecl : instantiatedDecls) {
        sortedInstantiatedDecls.insert(tempDecl);
    }
    for (auto& instance : sortedInstantiatedDecls) {
        CollectDecls(*instance, false);
    }
}

void AST2CHIR::CollectFuncDecl(AST::FuncDecl& funcDecl)
{
    // Do not collect intrinsic functions, in order to reduce the size of package.
    if (funcDecl.TestAttr(AST::Attribute::INTRINSIC)) {
        return;
    }

    if (funcDecl.TestAttr(AST::Attribute::FOREIGN)) {
        CollectDeclToList(funcDecl, foreignFuncs);
        return;
    }

    if (funcDecl.TestAttr(AST::Attribute::GENERIC)) {
        CollectInstantiatedDecls(funcDecl);
        // Collect the function
        CollectDeclToList(funcDecl, globalAndMemberFuncs);
        // We need to collect the func itself and the default param func if there is any
        for (auto& param : funcDecl.funcBody->paramLists[0]->params) {
            if (param->desugarDecl) {
                CollectDeclToList(*param->desugarDecl, globalAndMemberFuncs);
            }
        }
    } else if (funcDecl.outerDecl && funcDecl.outerDecl->ty->HasGeneric()) {
        // When the func is not generic decl, but if the outerDecl of the funcDecl is generic, we still need to
        // collect this funcDecl.
        CollectDeclToList(funcDecl, globalAndMemberFuncs);
        for (auto& param : funcDecl.funcBody->paramLists[0]->params) {
            /*
            class A<T> {
                var a:T
                init(b:T) {
                    a = b
                }
                func foo(c!:T = a, b!:Int64 = 2) {
                    return 0
                }
            }
            */
            if (param->desugarDecl) {
                CollectDeclToList(*param->desugarDecl, globalAndMemberFuncs);
            }
        }
    } else {
        // `instantiated` decl denote that: generic definition in up-stream pkg, but instantiated in current pkg, these
        // decls's IMPORTED Attribute is false,So we can't distinguish it from the current package decl.
        CollectDeclToList(funcDecl, globalAndMemberFuncs);
        for (auto& param : funcDecl.funcBody->paramLists[0]->params) {
            if (param->desugarDecl) {
                CollectDeclToList(*param->desugarDecl, globalAndMemberFuncs);
            }
        }
    }
}

void AST2CHIR::CollectMemberDecl(AST::Decl& decl)
{
    if (auto funcDecl = DynamicCast<AST::FuncDecl*>(&decl); funcDecl) {
        CollectFuncDecl(*funcDecl);
    } else if (auto propDecl = DynamicCast<AST::PropDecl*>(&decl); propDecl) {
        // Collect all the `getter` and `setter` functions
        for (auto& it : propDecl->getters) {
            CollectFuncDecl(*it);
        }
        for (auto& it : propDecl->setters) {
            CollectFuncDecl(*it);
        }
    } else if (auto varDecl = DynamicCast<AST::VarDecl*>(&decl); varDecl) {
        CJC_ASSERT(varDecl->TestAttr(AST::Attribute::STATIC));
        // VarWithPatternDecl can't be a static member decl, so we only deal with VarDecl.
        if (varDecl->outerDecl && varDecl->outerDecl->TestAttr(AST::Attribute::GENERIC)) {
            /* We should collect geneirc static member VarDecl.
                Cangjie code:
                interface I<T> {
                    static func get() : T
                }
                class A <: I<A> {
                    public static func get() : A { A() }
                }
                class B<T> where T <: I<T> {
                    static let v : T = T.get()
                }
            */
            CollectDeclToList(*varDecl, globalAndStaticVars);
        } else {
            CollectDeclToList(*varDecl, globalAndStaticVars);
        }
    }
}

void AST2CHIR::CollectDeclsFromStructDecl(const AST::StructDecl& structDecl)
{
    for (auto& member : structDecl.body->decls) {
        // Skip the non-static member variable declaration
        if (member->astKind == AST::ASTKind::VAR_DECL && !member.get()->TestAttr(AST::Attribute::STATIC)) {
            continue;
        }

        CollectMemberDecl(*member);
    }
}

void AST2CHIR::CollectDesugarDecl(AST::Decl& decl)
{
    if (decl.astKind == AST::ASTKind::MAIN_DECL) {
        auto mainDecl = StaticCast<AST::MainDecl*>(&decl);
        if (mainDecl->desugarDecl) {
            // The desugarDecl has the attribute:MAIN_ENTRY
            CollectFuncDecl(*mainDecl->desugarDecl);
        }
        return;
    }
    if (decl.astKind == AST::ASTKind::MACRO_DECL) {
        auto macroDecl = StaticCast<AST::MacroDecl*>(&decl);
        if (macroDecl->desugarDecl) {
            // The desugarDecl has the attribute:MACRO_FUNC
            CollectFuncDecl(*macroDecl->desugarDecl);
        }
    }
    return;
}

void AST2CHIR::CollectVarandVarwithpatternDecl(AST::Decl& decl)
{
    // global VarDecl and VarWithPatternDecl can't be generic decl.
    CollectDeclToList(decl, globalAndStaticVars);
}

void AST2CHIR::CollectDecls(AST::Decl& decl, bool instantiated)
{
    switch (decl.astKind) {
        case AST::ASTKind::EXTEND_DECL: {
            auto extendDecl = StaticCast<AST::ExtendDecl*>(&decl);
            if (extendDecl->TestAttr(AST::Attribute::GENERIC)) {
                CollectInstantiatedDecls(*extendDecl);
                CollectDeclsFromExtendDecl(*extendDecl);
                CollectDeclToList(*extendDecl, genericNominalDecls);
            } else {
                CollectDeclsFromExtendDecl(*StaticCast<AST::ExtendDecl*>(&decl));
                if (instantiated) {
                    CollectDeclToList(*extendDecl, importedGenericInstantiatedNominalDecls);
                } else {
                    CollectDeclToList(*extendDecl, nominalDecls);
                }
            }
            break;
        }
        case AST::ASTKind::STRUCT_DECL: {
            auto structDecl = StaticCast<AST::StructDecl*>(&decl);
            if (structDecl->TestAttr(AST::Attribute::GENERIC)) {
                CollectInstantiatedDecls(*structDecl);
                CollectDeclsFromStructDecl(*structDecl);
                CollectDeclToList(*structDecl, genericNominalDecls);
            } else {
                CollectDeclsFromStructDecl(*structDecl);
                if (instantiated) {
                    CollectDeclToList(*structDecl, importedGenericInstantiatedNominalDecls);
                } else {
                    CollectDeclToList(*structDecl, nominalDecls);
                }
            }
            break;
        }
        case AST::ASTKind::INTERFACE_DECL:
        case AST::ASTKind::CLASS_DECL: {
            auto classLikeDecl = StaticCast<AST::ClassLikeDecl*>(&decl);
            if (classLikeDecl->TestAttr(AST::Attribute::GENERIC)) {
                CollectInstantiatedDecls(*classLikeDecl);
                CollectDeclsFromClassLikeDecl(*classLikeDecl);
                CollectDeclToList(*classLikeDecl, genericNominalDecls);
            } else {
                CollectDeclsFromClassLikeDecl(*classLikeDecl);
                if (instantiated) {
                    CollectDeclToList(*classLikeDecl, importedGenericInstantiatedNominalDecls);
                } else {
                    CollectDeclToList(*classLikeDecl, nominalDecls);
                }
            }
            break;
        }
        case AST::ASTKind::ENUM_DECL: {
            auto enumDecl = StaticCast<AST::EnumDecl*>(&decl);
            if (enumDecl->TestAttr(AST::Attribute::GENERIC)) {
                CollectInstantiatedDecls(*enumDecl);
                CollectDeclsFromEnumDecl(*enumDecl);
                CollectDeclToList(*enumDecl, genericNominalDecls);
            } else {
                CollectDeclsFromEnumDecl(*enumDecl);
                if (instantiated) {
                    CollectDeclToList(*enumDecl, importedGenericInstantiatedNominalDecls);
                } else {
                    CollectDeclToList(*enumDecl, nominalDecls);
                }
            }
            break;
        }
        case AST::ASTKind::FUNC_DECL: {
            CollectFuncDecl(StaticCast<AST::FuncDecl&>(decl));
            break;
        }
        case AST::ASTKind::MAIN_DECL:
        case AST::ASTKind::MACRO_DECL: {
            CollectDesugarDecl(decl);
            break;
        }
        case AST::ASTKind::VAR_DECL:
        case AST::ASTKind::VAR_WITH_PATTERN_DECL: {
            CollectVarandVarwithpatternDecl(decl);
            break;
        }
        default: {
            // Ignore other decls.
        }
    }
}

void AST2CHIR::CollectImportedGenericInstantiatedDecl(
    const AST::Package& node, std::unordered_set<std::string>& mangledNameSet)
{
    for (auto& decl : node.genericInstantiatedDecls) {
        auto genericDecl = decl->genericDecl;

        // We collect imported Instantiated decls in the current step, and Instantiated decls in current package will be
        // collected by `CollectDeclsInCurPkg`.
        // we can't check the `IMPORTED` attribute of `genericDecl` directly,
        // cause if we have code: `classC<T>.foo<U>`,the instantiated version of classC<T> will change the
        // fullPackageName, so should not use genericDecl->fullPackageName.
        if (genericDecl->curFile->curPackage->fullPackageName == node.fullPackageName) {
            continue;
        }

        // We should not collect inner function.
        if (auto funcDecl = DynamicCast<AST::FuncDecl*>(genericDecl); funcDecl) {
            if (auto outerDecl = DynamicCast<AST::FuncDecl*>(funcDecl->outerDecl); outerDecl) {
                continue;
            }
        }

        // not collect different decl with same mangled name
        if (decl->astKind != AST::ASTKind::EXTEND_DECL) {
            auto res = mangledNameSet.emplace(decl->mangledName).second;
            if (!res) {
                continue;
            }
        }
        CollectDecls(*decl, true);
    }
}

void AST2CHIR::CollectImportedFuncDeclAndDesugarParams(AST::FuncDecl& funcDecl)
{
    if (IsSrcCodeImportedGlobalDecl(funcDecl, opts)) {
        CollectFuncDeclToList(funcDecl, globalAndMemberFuncs);
    } else {
        CollectFuncDeclToList(funcDecl, importedGlobalAndMemberFuncs);
    }
}

void AST2CHIR::CollectImportedGlobalOrStaticVarDecl(AST::VarDecl& varDecl)
{
    if (IsSrcCodeImportedGlobalDecl(varDecl, opts)) {
        CollectDeclToList(varDecl, globalAndStaticVars);
    } else {
        CollectDeclToList(varDecl, importedGlobalAndStaticVars);
    }
}

void AST2CHIR::CollectImportedGenericDecl(AST::Decl& decl)
{
    // 1. imported generic func decl
    // 2. imported generic nominal decl
    //   2.1 member var decl of imported generic nominal decl
    //   2.2 member func decl of imported generic nominal decl
    if (decl.astKind == AST::ASTKind::FUNC_DECL) {
        CollectImportedFuncDeclAndDesugarParams(StaticCast<AST::FuncDecl&>(decl));
    } else if (decl.IsNominalDecl()) {
        CollectDeclToList(decl, importedNominalDecls);
        for (auto& d : decl.GetMemberDecls()) {
            CollectImportedGenericDecl(*d);
        }
    } else if (decl.astKind == AST::ASTKind::PROP_DECL) {
        CollectImportedPropDecl(StaticCast<AST::PropDecl&>(decl));
    } else if (decl.astKind == AST::ASTKind::VAR_DECL && decl.TestAttr(AST::Attribute::STATIC)) {
        CollectImportedGlobalOrStaticVarDecl(StaticCast<AST::VarDecl&>(decl));
    }
}

void AST2CHIR::CollectFuncDeclToList(AST::FuncDecl& func, std::vector<Ptr<const AST::Decl>>& list)
{
    CollectDeclToList(func, list);
    for (auto& param : func.funcBody->paramLists[0]->params) {
        if (param->desugarDecl) {
            CollectDeclToList(*param->desugarDecl, list);
        }
    }
}

void AST2CHIR::CollectImportedPropDecl(AST::PropDecl& propDecl)
{
    for (auto& it : propDecl.getters) {
        CollectImportedFuncDeclAndDesugarParams(StaticCast<AST::FuncDecl&>(*it));
    }
    for (auto& it : propDecl.setters) {
        CollectImportedFuncDeclAndDesugarParams(StaticCast<AST::FuncDecl&>(*it));
    }
}

void AST2CHIR::CollectImportedDeclUsedInCurPkg(AST::Decl& decl)
{
    // don't need to collect intrinsic func decl, because in CHIR, if we visit an intrinsic call expr, we will create
    // an `intrinsic` expression of CHIR, and pass in intrinsic kind, then codegen will know which function need to be
    // called. we don't need to translate intrinsic func decl to any CHIR node
    if (decl.TestAttr(AST::Attribute::INTRINSIC)) {
        return;
    }
    // 1. imported generic decl
    // 2. source imported var decl and func decl(func decl must be inlinable)
    // 3. imported nominal decl
    //   3.1 member var decl of imported nominal decl
    //   3.2 member func decl of imported nominal decl
    // 4. imported func decl
    // 5. imported prop decl
    // 6. imported var decl and var with pattern decl
    if (decl.TestAttr(AST::Attribute::GENERIC) ||
        (decl.outerDecl != nullptr && decl.outerDecl->TestAttr(AST::Attribute::GENERIC))) {
        CollectImportedGenericDecl(decl);
    } else if (decl.IsNominalDecl()) {
        CollectDeclToList(decl, importedNominalDecls);
        for (auto& d : decl.GetMemberDecls()) {
            CollectImportedDeclUsedInCurPkg(*d);
        }
    } else if (decl.IsFunc()) {
        CollectImportedFuncDeclAndDesugarParams(StaticCast<AST::FuncDecl&>(decl));
    } else if (decl.astKind == AST::ASTKind::PROP_DECL) {
        CollectImportedPropDecl(StaticCast<AST::PropDecl&>(decl));
    } else if (decl.astKind == AST::ASTKind::VAR_DECL &&
        decl.TestAnyAttr(AST::Attribute::GLOBAL, AST::Attribute::STATIC)) {
        CollectImportedGlobalOrStaticVarDecl(StaticCast<AST::VarDecl&>(decl));
    }
}

void AST2CHIR::CollectImportedDecls(const AST::Package& node)
{
    usedSrcImportedNonGenericDecls = std::unordered_set<Ptr<const AST::Decl>>{
        node.srcImportedNonGenericDecls.begin(), node.srcImportedNonGenericDecls.end()};
    std::unordered_set<std::string> mangledNameSet;
    // 1. imported generic instantiated declarations, for which we should collect their instantiated versions
    CollectImportedGenericInstantiatedDecl(node, mangledNameSet);

    // 2. all imported decls, only including used decls in current package
    for (auto& importPkg : importManager.GetAllImportedPackages()) {
        CJC_NULLPTR_CHECK(importPkg->srcPackage.get());
        // Exclude current package.
        if (importPkg->srcPackage == &node) {
            continue;
        }
        // used in current package
        for (auto& file : importPkg->srcPackage->files) {
            for (auto& decl : file->decls) {
                CollectImportedDeclUsedInCurPkg(*decl);
            }
            for (auto& decl : file->exportedInternalDecls) {
                CollectImportedDeclUsedInCurPkg(*decl);
            }
        }
    }
}

void AST2CHIR::CollectDeclsInCurPkg(AST::Package& node)
{
    for (auto& file : node.files) {
        for (auto& decl : file->decls) {
            CollectDecls(*decl, false);
        }
    }
}

void AST2CHIR::SetFuncAttributeAndLinkageType(const AST::FuncDecl& astFunc, FuncBase& chirFunc)
{
    // 1. ----------------------- Attribute -----------------------
    chirFunc.AppendAttributeInfo(BuildAttr(astFunc.GetAttrs()));
    if (astFunc.isConst) {
        chirFunc.EnableAttr(Attribute::CONST);
    }
    // in SEMA, if a local const func is declared in static member method, it will be set STATIC
    // STATIC can be set for local func in SEMA, but not in CHIR, especially for const local func,
    // it can be lifted to global func, we need to disable STATIC, otherwise, a wrong Func will be generated in CHIR
    if (IsLocalConstFuncInStaticMember(astFunc)) {
        chirFunc.DisableAttr(Attribute::STATIC);
    }
    // in CHIR, we treat `static.init()` as global function, not member function,
    // because its outerDecl is something like `class A<T>`, if it's member function,
    // initializer will be translated as follows:
    // Func gv$_init() {
    //     Apply(static.init)(A<T>, [], Unit) // `T` is not declared in this scope
    // }
    // we still store its outerDecl in Sema, because it's used in `GlobalVarSort`
    if (IsStaticInit(astFunc)) {
        chirFunc.DisableAttr(Attribute::STATIC);
        chirFunc.DisableAttr(Attribute::PRIVATE);
        chirFunc.EnableAttr(Attribute::INTERNAL);
    }
    if (kind == IncreKind::INCR && !astFunc.toBeCompiled) {
        chirFunc.EnableAttr(Attribute::NON_RECOMPILE);
    }
    if (astFunc.TestAttr(AST::Attribute::IN_EXTEND) || astFunc.TestAttr(AST::Attribute::UNSAFE) ||
        astFunc.TestAttr(AST::Attribute::GENERIC_INSTANTIATED) || !astFunc.TestAttr(AST::Attribute::PUBLIC)) {
        chirFunc.EnableAttr(Attribute::NO_REFLECT_INFO);
    }
    // 1. function `main`, generated by user
    // 2. function `$mainInvoke`(mangled name is `user.main`), generated by compiler
    // 3. function which needed by runtime(in map `SPECIAL_FUNC_NAMES`, mangled name is begin with `rt$`)
    if (astFunc.TestAttr(AST::Attribute::MAIN_ENTRY) || astFunc.mangledName == USER_MAIN_MANGLED_NAME ||
        astFunc.mangledName.find("rt$") == 0) {
        chirFunc.EnableAttr(Attribute::NO_INLINE);
    }

    // 2. ----------------------- LinkageType -----------------------
    chirFunc.Set<LinkTypeInfo>(GetFuncLinkage(opts, astFunc));

    // 3. ----------------------- Others -----------------------
    chirFunc.SetFuncKind(GetFuncKindFromAST(astFunc));
    if (chirFunc.GetFuncKind() == FuncKind::DEFAULT_PARAMETER_FUNC) {
        chirFunc.SetParamDftValHostFunc(*VirtualCast<FuncBase*>(globalCache.Get(*astFunc.ownerFunc)));
    }
    chirFunc.SetFastNative(astFunc.isFastNative);
}

void AST2CHIR::CreateFuncSignatureAndSetGlobalCache(const AST::FuncDecl& funcDecl)
{
    if (funcDecl.TestAttr(AST::Attribute::GENERIC)) {
        TranslateFunctionGenericUpperBounds(chirType, funcDecl);
    }
    if (kind == IncreKind::INCR && !funcDecl.toBeCompiled && !IsSrcCodeImportedGlobalDecl(funcDecl, opts)) {
        CreatePseudoImportedFuncSignatureAndSetGlobalCache(funcDecl);
        return;
    }

    // when the callee of callExpr is abstract func, will create a `Invoke` node, so we don't need to put abstract
    // func into the `globalCache`,
    if (funcDecl.TestAttr(AST::Attribute::ABSTRACT)) {
        return;
    }
    auto fnTy = chirType.TranslateType(*funcDecl.ty);
    fnTy = AdjustFuncType(*StaticCast<FuncType*>(fnTy), funcDecl, builder, chirType);
    // Create BlockGroup with argument

    FuncType* funcTy = StaticCast<FuncType*>(fnTy);
    const auto& loc = DebugLocation(
        TranslateLocationWithoutScope(builder.GetChirContext(), funcDecl.begin, funcDecl.end));
    auto genericParamTy = GetGenericParamType(funcDecl, chirType);
    // Global or member function. Must not be nested func.
    auto pkgName = GetNameOfDefinedPackage(funcDecl);
    auto mangledName = funcDecl.mangledName;
    // there is a strange func decl in macro related package, such as `std.unittest.testmacro.a`
    // func name is macroCall_[a/c]_{name}_{packageName}, it's compiler added and not expected to mangle
    // but its mangled name and src name must be different, or we will get duplicated mangled name after cffi wrapper
    // we need to fix this hack way
    if (funcDecl.TestAttr(AST::Attribute::NO_MANGLE) && funcTy->IsCFunc()) {
        mangledName += CFFI_FUNC_SUFFIX;
    }
    auto srcCodeName = funcDecl.identifier;
    auto rawMangledName = funcDecl.rawMangleName;
    Func* fn = builder.CreateFunc(loc, funcTy, mangledName, srcCodeName, rawMangledName, pkgName, genericParamTy);
    BlockGroup* body = builder.CreateBlockGroup(*fn);
    fn->InitBody(*body);

    CJC_ASSERT(fn);
    SetFuncAttributeAndLinkageType(funcDecl, *fn);
    if (IsSrcCodeImportedGlobalDecl(funcDecl, opts)) {
        srcCodeImportedFuncs.emplace(fn);
    }
    std::vector<DebugLocation> paramLoc;
    if (IsInstanceMember(funcDecl)) {
        paramLoc.emplace_back(INVALID_LOCATION);
    }
    for (auto& astParam : funcDecl.funcBody->paramLists[0]->params) {
        paramLoc.emplace_back(TranslateLocationWithoutScope(builder.GetChirContext(), astParam->begin, astParam->end));
    }
    auto paramTypes = funcTy->GetParamTypes();
    CJC_ASSERT(paramTypes.size() == paramLoc.size());
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        builder.CreateParameter(paramTypes[i], paramLoc[i], *fn);
    }

    if (implicitDecls.count(&funcDecl) != 0) {
        implicitFuncs.emplace(fn->GetIdentifierWithoutPrefix(), fn);
    }
    globalCache.Set(funcDecl, *fn);

    // collect annotation info, and create anno factory func
    // do this here rather than in Func translation, because this function is run serialised but TranslateFuncDecl
    // is done in parallel
    if (funcDecl.TestAttr(AST::Attribute::GLOBAL) && !funcDecl.TestAttr(AST::Attribute::GENERIC_INSTANTIATED)) {
        auto tr = CreateTranslator();
        tr.CreateAnnoFactoryFuncsForFuncDecl(funcDecl, nullptr);
        tr.CollectValueAnnotation(funcDecl);
    }
}

void AST2CHIR::CreatePseudoImportedFuncSignatureAndSetGlobalCache(const AST::FuncDecl& funcDecl)
{
    // when the callee of callExpr is abstract func, will create a `Invoke` node, so we don't need to put abstract
    // func into the `globalCache`,
    if (funcDecl.TestAttr(AST::Attribute::ABSTRACT)) {
        return;
    }
    auto fnTy = chirType.TranslateType(*funcDecl.ty);
    fnTy = AdjustFuncType(*StaticCast<FuncType*>(fnTy), funcDecl, builder, chirType);
    FuncType* funcTy = StaticCast<FuncType*>(fnTy);
    auto genericParamTy = GetGenericParamType(funcDecl, chirType);
    // Global or member function. Must not be nested func.
    auto fn = builder.CreateImportedVarOrFunc<ImportedFunc>(funcTy, funcDecl.mangledName, funcDecl.identifier,
        funcDecl.rawMangleName, funcDecl.fullPackageName, genericParamTy);
    CJC_ASSERT(fn);
    SetFuncAttributeAndLinkageType(funcDecl, *fn);

    CJC_ASSERT(funcDecl.funcBody->paramLists.size() == 1);
    auto& funcParams = funcDecl.funcBody->paramLists[0]->params;
    std::vector<AbstractMethodParam> paramsInfo;
    size_t idx = 0;
    auto paramTys = StaticCast<AST::FuncTy*>(funcDecl.ty)->paramTys;
    // NOTE: 'AnnoInfo' will be added during translating customDef.
    std::for_each(funcParams.begin(), funcParams.end(),
        [this, &paramsInfo, &idx, paramTys](const OwnedPtr<AST::FuncParam>& param) {
            paramsInfo.emplace_back(
                AbstractMethodParam{param->identifier, chirType.TranslateType(*paramTys[idx]), AnnoInfo()});
            ++idx;
        });
    if (IsInstanceMember(funcDecl)) {
        paramsInfo.insert(paramsInfo.begin(), {"this", funcTy->GetParamTypes()[0], {}});
    }
    fn->SetParamInfo(std::move(paramsInfo));
    if (implicitDecls.count(&funcDecl) != 0) {
        implicitFuncs.emplace(fn->GetIdentifierWithoutPrefix(), fn);
    }
    globalCache.Set(funcDecl, *fn);
}

void AST2CHIR::CreateImportedFuncSignatureAndSetGlobalCache(const AST::FuncDecl& funcDecl)
{
    if (funcDecl.TestAttr(AST::Attribute::GENERIC)) {
        TranslateFunctionGenericUpperBounds(chirType, funcDecl);
    }
    auto fnTy = chirType.TranslateType(*funcDecl.ty);
    fnTy = AdjustFuncType(*StaticCast<FuncType*>(fnTy), funcDecl, builder, chirType);
    auto genericParamTy = GetGenericParamType(funcDecl, chirType);
    auto fn = builder.CreateImportedVarOrFunc<ImportedFunc>(fnTy, funcDecl.mangledName, funcDecl.identifier,
        funcDecl.rawMangleName, funcDecl.fullPackageName, genericParamTy);
    CJC_NULLPTR_CHECK(fn);
    auto loc = TranslateLocationWithoutScope(builder.GetChirContext(), funcDecl.begin, funcDecl.end);
    fn->SetDebugLocation(loc);
    SetFuncAttributeAndLinkageType(funcDecl, *fn);
    if (implicitDecls.count(&funcDecl) != 0) {
        implicitFuncs.emplace(fn->GetIdentifierWithoutPrefix(), fn);
    }
    // set param infos of imported func
    auto paramTys = StaticCast<AST::FuncTy*>(funcDecl.ty)->paramTys;
    const auto& funcParams = funcDecl.funcBody->paramLists[0]->params;
    std::vector<AbstractMethodParam> paramsInfo;
    size_t idx = 0;
    std::for_each(funcParams.begin(), funcParams.end(),
        [this, &paramsInfo, &idx, paramTys](const OwnedPtr<AST::FuncParam>& param) {
            paramsInfo.emplace_back(AbstractMethodParam{param->identifier, chirType.TranslateType(*paramTys[idx]), {}});
            ++idx;
        });
    FuncType* funcTy = StaticCast<FuncType*>(fnTy);
    if (IsInstanceMember(funcDecl)) {
        paramsInfo.insert(paramsInfo.begin(), {"this", funcTy->GetParamTypes()[0], {}});
    }
    fn->SetParamInfo(std::move(paramsInfo));
    globalCache.Set(funcDecl, *fn);
}

void AST2CHIR::CreateImportedValueSignatureAndSetGlobalCache(const AST::VarDecl& varDecl)
{
    auto varType = chirType.TranslateType(*varDecl.ty);
    auto refTy = builder.GetType<RefType>(varType);
    auto var = builder.CreateImportedVarOrFunc<ImportedVar>(refTy, varDecl.mangledName, varDecl.identifier,
        varDecl.rawMangleName, varDecl.fullPackageName);
    CJC_NULLPTR_CHECK(var);
    var->AppendAttributeInfo(BuildAttr(varDecl.GetAttrs()));
    if (varDecl.IsConst()) {
        var->EnableAttr(Attribute::CONST);
    }

    globalCache.Set(varDecl, *var);
}

void AST2CHIR::CreateAndCacheGlobalVar(const AST::VarDecl& decl, bool isLocalConst)
{
    auto ty = builder.GetType<RefType>(chirType.TranslateType(*decl.ty));
    auto loc = TranslateLocationWithoutScope(builder.GetChirContext(), decl.begin, decl.end);
    auto warnPos = GetVarLoc(builder.GetChirContext(), decl);
    Value* gv = nullptr;
    auto mangledName = decl.mangledName;
    auto srcCodeName = decl.identifier;
    auto rawMangledName = decl.rawMangleName;
    auto packageName = decl.fullPackageName;
    if (kind == IncreKind::INCR && !decl.toBeCompiled && !IsSrcCodeImportedGlobalDecl(decl, opts)) {
        gv = builder.CreateImportedVarOrFunc<ImportedVar>(ty, mangledName, srcCodeName, rawMangledName, packageName);
    } else {
        gv = builder.CreateGlobalVar(loc, ty, mangledName, srcCodeName, rawMangledName, packageName);
        if (isLocalConst) {
            // use COMPILER_ADD to mark this global const var as lifted
            gv->EnableAttr(Attribute::COMPILER_ADD);
        }
    }
    if (kind == IncreKind::INCR && !decl.toBeCompiled) {
        gv->EnableAttr(Attribute::NON_RECOMPILE);
    }
    gv->Set<DebugLocationInfoForWarning>(warnPos);
    gv->AppendAttributeInfo(BuildVarDeclAttr(decl));
    SetGlobalVarLinkageType(*gv, decl, creatingLocalConstVarSignature, opts.target.os == Triple::OSType::WINDOWS);
    if (decl.IsConst()) {
        gv->EnableAttr(Attribute::CONST);
    }
    if (IsSrcCodeImportedGlobalDecl(decl, opts)) {
        srcCodeImportedVars.emplace(VirtualCast<GlobalVar*>(gv));
    }
    globalCache.Set(decl, *gv);
}

void AST2CHIR::CacheTopLevelDeclToGlobalSymbolTable()
{
    // create imported func decls and var decls.
    for (auto& decl : foreignFuncs) {
        CJC_ASSERT(decl->astKind == AST::ASTKind::FUNC_DECL);
        CreateImportedFuncSignatureAndSetGlobalCache(*StaticCast<const AST::FuncDecl*>(decl));
    }
    for (auto& decl : importedGlobalAndMemberFuncs) {
        CJC_ASSERT(decl->astKind == AST::ASTKind::FUNC_DECL);
        CreateImportedFuncSignatureAndSetGlobalCache(*StaticCast<const AST::FuncDecl *>(decl));
    }
    for (auto& decl : importedGlobalAndStaticVars) {
        CJC_ASSERT(decl->astKind == AST::ASTKind::VAR_DECL);
        CreateImportedValueSignatureAndSetGlobalCache(*StaticCast<const AST::VarDecl*>(decl));
    }

    // create current packages's func decls and var decls.
    for (auto& decl : globalAndMemberFuncs) {
        CJC_ASSERT(decl->astKind == AST::ASTKind::FUNC_DECL);
        CreateFuncSignatureAndSetGlobalCache(*StaticCast<AST::FuncDecl*>(decl));
    }

    CreateGlobalVarSignature(globalAndStaticVars);
    creatingLocalConstVarSignature = true;
    
    // collect Annotation of global vars
    auto tr = CreateTranslator();
    for (auto var : globalAndStaticVars) {
        if (!var->TestAttr(AST::Attribute::STATIC)) {
            auto fn = tr.CreateAnnoFactoryFuncSig(*var, nullptr);
            if (fn.mangledName != "none" && Is<AST::VarDecl>(var)) {
                globalCache.Get(*var)->SetAnnoInfo(std::move(fn));
            }
            tr.CollectValueAnnotation(*var);
        }
    }

    CreateGlobalVarSignature(localConstVars.stableOrderValue, true);
    creatingLocalConstVarSignature = false;
    for (auto decl : std::as_const(localConstFuncs.stableOrderValue)) {
        CreateFuncSignatureAndSetGlobalCache(*decl);
    }
}

void AST2CHIR::CreatePseudoDefForAnnoOnlyDecl(const AST::Decl& decl)
{
    if (chirType.Has(decl) || globalCache.TryGet(decl)) {
        return;
    }
    auto tr = CreateTranslator();
    CustomTypeDef* sym{};
    if (auto cl = DynamicCast<AST::ClassLikeDecl>(&decl)) {
        sym = builder.CreateClass(tr.TranslateLocation(decl), decl.identifier.Val(), decl.mangledName,
            decl.fullPackageName, Is<AST::ClassDecl>(cl), false);
    }
    if (auto cl = DynamicCast<AST::StructDecl>(&decl)) {
        sym = builder.CreateStruct(
            tr.TranslateLocation(decl), decl.identifier.Val(), decl.mangledName, decl.fullPackageName, false);
    }
    if (auto cl = DynamicCast<AST::EnumDecl>(&decl)) {
        sym = builder.CreateEnum(tr.TranslateLocation(decl), decl.identifier.Val(), decl.mangledName,
            decl.fullPackageName, false, cl->hasEllipsis);
    }
    if (auto cl = DynamicCast<AST::ExtendDecl>(&decl)) {
        sym = builder.CreateExtend(tr.TranslateLocation(decl), decl.mangledName, decl.fullPackageName, false);
    }
    if (sym) {
        sym->EnableAttr(Attribute::SKIP_ANALYSIS);
        chirType.SetGlobalNominalCache(decl, *sym);
        return;
    }
    Value* val;
    if (auto func = DynamicCast<AST::FuncDecl>(&decl)) {
        auto fn = builder.CreateFunc(INVALID_LOCATION,
            builder.GetType<FuncType>(
                std::vector<Type*>{func->funcBody->paramLists[0]->params.size(), builder.GetInt64Ty()},
                builder.GetUnitTy()),
            decl.mangledName, decl.mangledName, decl.rawMangleName, decl.fullPackageName);
        for (auto& pr : func->funcBody->paramLists[0]->params) {
            builder.CreateParameter(builder.GetInt64Ty(), tr.TranslateLocation(*pr), *fn);
        }
        val = fn;
    }
    if (auto var = DynamicCast<AST::VarDecl>(&decl)) {
        val = builder.CreateGlobalVar(tr.TranslateLocation(decl), builder.GetType<RefType>(builder.GetInt64Ty()),
            decl.mangledName, decl.identifier, decl.rawMangleName, decl.fullPackageName);
    }
    // such function does not have body (because it is a pseudo func, just a hook for annoFactoryFunc).
    // skip all CHIR pass on it.
    val->EnableAttr(Attribute::SKIP_ANALYSIS);
    globalCache.Set(decl, *val);
}

void AST2CHIR::CreateAnnoOnlyDeclSig(const AST::Decl& decl)
{
    // pseudo def of inheritable decls are create earlier in CacheCustomTypeDefToGlobalSymbolTable; do not create twice
    if (auto type = DynamicCast<AST::InheritableDecl>(&decl)) {
        for (auto& member : type->GetMemberDecls()) {
            CreateAnnoOnlyDeclSig(*member);
        }
    } else {
        CreatePseudoDefForAnnoOnlyDecl(decl);
    }
    auto tr = CreateTranslator();
    if (auto funcDecl = DynamicCast<AST::FuncDecl>(&decl)) {
        tr.CreateAnnoFactoryFuncsForFuncDecl(*funcDecl, nullptr);
    } else {
        auto fn = tr.CreateAnnoFactoryFuncSig(decl, nullptr);
        if (fn.mangledName != "none") {
            if (auto var = DynamicCast<AST::VarDecl>(&decl)) {
                globalCache.Get(*var)->SetAnnoInfo(std::move(fn));
            }
        }
    }
}

static void SetCustomTypeDefAttr(CustomTypeDef& def, const AST::Decl& decl)
{
    def.AppendAttributeInfo(BuildAttr(decl.GetAttrs()));
    if (auto classDef = DynamicCast<ClassDef*>(&def)) {
        if (decl.TestAttr(AST::Attribute::IS_ANNOTATION)) {
            classDef->SetAnnotation(true);
        }
    } else if (auto structDef = DynamicCast<StructDef*>(&def)) {
        if (decl.TestAttr(AST::Attribute::C)) {
            structDef->SetCStruct(true);
        }
    }
}

void AST2CHIR::CreateCustomTypeDef(const AST::Decl& decl, bool isImported)
{
    CJC_ASSERT(decl.IsNominalDecl());
    CustomTypeDef* customTypeDef = nullptr;
    auto loc = TranslateLocationWithoutScope(builder.GetChirContext(), decl.begin, decl.end);
    auto& identifier = decl.identifier.Val();
    auto& mangledName = decl.mangledName;
    auto& pkgName = decl.genericDecl ? decl.genericDecl->fullPackageName : decl.fullPackageName;
    AST::Decl* uniqueDecl = nullptr;
    switch (decl.astKind) {
        case AST::ASTKind::CLASS_DECL:
            customTypeDef = builder.CreateClass(loc,
                identifier, mangledName, pkgName, true, isImported);
            uniqueDecl = StaticCast<AST::ClassLikeTy*>(decl.ty)->commonDecl;
            break;
        case AST::ASTKind::INTERFACE_DECL:
            customTypeDef = builder.CreateClass(loc,
                identifier, mangledName, pkgName, false, isImported);
            uniqueDecl = StaticCast<AST::ClassLikeTy*>(decl.ty)->commonDecl;
            break;
        case AST::ASTKind::STRUCT_DECL:
            customTypeDef =
                builder.CreateStruct(loc, identifier, mangledName, pkgName, isImported);
            uniqueDecl = StaticCast<AST::StructTy*>(decl.ty)->decl;
            break;
        case AST::ASTKind::ENUM_DECL:
            customTypeDef = builder.CreateEnum(loc, identifier, mangledName, pkgName, isImported,
                StaticCast<AST::EnumDecl>(decl).hasEllipsis);
            uniqueDecl = StaticCast<AST::EnumTy*>(decl.ty)->decl;
            break;
        case AST::ASTKind::EXTEND_DECL: {
            auto gts = GetGenericParamType(decl, chirType);
            customTypeDef = builder.CreateExtend(loc, mangledName, pkgName, isImported, gts);
            break;
        }
        default:
            break;
    }
    CJC_NULLPTR_CHECK(customTypeDef);
    // For incremental compilation, compiler added instantiated decl may need to be ignored.
    if (kind == Cangjie::IncreKind::INCR && decl.TestAttr(Cangjie::AST::Attribute::GENERIC_INSTANTIATED) &&
        !decl.toBeCompiled) {
        customTypeDef->EnableAttr(Attribute::NON_RECOMPILE);
    }
    SetCustomTypeDefAttr(*customTypeDef, decl);
    chirType.SetGlobalNominalCache(decl, *customTypeDef);
    if (uniqueDecl != nullptr && uniqueDecl != &decl) {
        chirType.SetGlobalNominalCache(*uniqueDecl, *customTypeDef);
    }
}

void AST2CHIR::CacheCustomTypeDefToGlobalSymbolTable()
{
    for (auto decl : nominalDecls) {
        CreateCustomTypeDef(*decl, false);
    }
    for (auto decl : genericNominalDecls) {
        CreateCustomTypeDef(*decl, false);
    }
    for (auto decl : importedNominalDecls) {
        CreateCustomTypeDef(*decl, true);
    }
    for (auto decl : importedGenericInstantiatedNominalDecls) {
        CreateCustomTypeDef(*decl, false);
    }
    for (auto decl : annoOnlyDecls) {
        if (Is<AST::InheritableDecl>(decl)) {
            CreatePseudoDefForAnnoOnlyDecl(*decl);
        }
    }
}

void AST2CHIR::TranslateAllCustomTypeTy()
{
    auto translateNow = [](const AST::Decl& decl) {
        if (decl.astKind == AST::ASTKind::CLASS_DECL || decl.astKind == AST::ASTKind::INTERFACE_DECL ||
            decl.astKind == AST::ASTKind::STRUCT_DECL || decl.astKind == AST::ASTKind::ENUM_DECL) {
            return true;
        }
        return false;
    };

    std::unordered_set<Ptr<const AST::Decl>> translatedGenericDecls;
    for (auto decl : importedNominalDecls) {
        if (!translateNow(*decl)) {
            continue;
        }
        auto type = chirType.TranslateType(*(decl->ty));
        if (decl->TestAttr(AST::Attribute::GENERIC)) {
            translatedGenericDecls.insert(decl);
        }
        if (decl->identifier == OBJECT_NAME && decl->fullPackageName == CORE_PACKAGE_NAME) {
            auto clsTy = StaticCast<ClassType*>(StaticCast<RefType*>(type)->GetBaseType());
            builder.SetObjectTy(clsTy);
        }
        if (decl->identifier == ANY_NAME && decl->fullPackageName == CORE_PACKAGE_NAME) {
            auto clsTy = StaticCast<ClassType*>(StaticCast<RefType*>(type)->GetBaseType());
            builder.SetAnyTy(clsTy);
        }
    }
    for (auto decl : genericNominalDecls) {
        if (translateNow(*decl)) {
            chirType.TranslateType(*(decl->ty));
            translatedGenericDecls.emplace(decl);
        }
    }
    for (auto decl : nominalDecls) {
        if (!translateNow(*decl)) {
            continue;
        }
        auto type = chirType.TranslateType(*(decl->ty));
        if (decl->identifier == OBJECT_NAME && decl->fullPackageName == CORE_PACKAGE_NAME) {
            auto clsTy = StaticCast<ClassType*>(StaticCast<RefType*>(type)->GetBaseType());
            builder.SetObjectTy(clsTy);
        }
        if (decl->identifier == ANY_NAME && decl->fullPackageName == CORE_PACKAGE_NAME) {
            auto clsTy = StaticCast<ClassType*>(StaticCast<RefType*>(type)->GetBaseType());
            builder.SetAnyTy(clsTy);
        }
    }
    // Translate upper bounds after normal decls have been translated.
    for (auto decl : translatedGenericDecls) {
        for (auto ty : decl->ty->typeArgs) {
            chirType.FillGenericArgType(*RawStaticCast<AST::GenericsTy*>(ty));
        }
    }
    for (auto decl : importedGenericInstantiatedNominalDecls) {
        if (translateNow(*decl)) {
            chirType.TranslateType(*(decl->ty));
        }
    }
}

static bool ParentIsOtherExtendGrandParent(const ClassType& parent, ExtendDef& curDef, CHIRBuilder& builder)
{
    auto extendDefs = curDef.GetExtendedType()->GetExtends(&builder);
    Utils::RemoveFromVec(extendDefs, &curDef);
    std::unordered_set<const ClassType*> allGrandParents;
    for (auto def : extendDefs) {
        for (auto extendParent : def->GetImplementedInterfaceTys()) {
            auto grandParents = extendParent->GetSuperTypesRecusively(builder);
            allGrandParents.insert(grandParents.begin(), grandParents.end());
        }
    }
    if (allGrandParents.find(&parent) != allGrandParents.end()) {
        return true;
    }
    auto curParentTypeArgs = parent.GetGenericArgs();
    auto curParentDef = parent.GetClassDef();
    for (auto grandParent : allGrandParents) {
        if (grandParent->GetClassDef() != curParentDef) {
            continue;
        }
        auto grandParentTypeArgs = grandParent->GetGenericArgs();
        CJC_ASSERT(curParentTypeArgs.size() == grandParentTypeArgs.size());
        for (size_t i = 0; i < curParentTypeArgs.size(); ++i) {
            auto typeArg1 = curParentTypeArgs[i];
            auto typeArg2 = grandParentTypeArgs[i];
            if (typeArg1 != typeArg2 && !typeArg1->IsGeneric() && !typeArg2->IsGeneric()) {
                return false;
            }
        }
        // I<T, Bool> and I<U, Bool> is type matched
        return true;
    }
    return false;
}

void AST2CHIR::UpdateExtendParent()
{
    /* we will remove some parent from extend def. e.g.
        interface I1       { func foo() {println(1)} }
        interface I2 <: I1 { func foo() {println(2)} }
        class A {}
        extend A <: I1 {
            vtable { I1 -> I1::foo }
        }
        extend A <: I2 {
            vtable { I1 -> I2::foo }
        }
        what about result of `A().foo()` ?
        in this case, it's hard to say which `foo` will be called, I1::foo or I2::foo ?
        we need to let cangjie users know that it must be I2::foo, not I1::foo,
        so vtable in `extend A <: I1` must be removed, but it's not enough
        codegen will generate an empty extension def of `A_ed_I1` according to `extend A <: I1`,
        and a non-empty extension def of `A_ed_I1` according to `extend A <: I2`
        codegen need to know inheritance relationship by extension def, so even though there isn't vtable
        in `extend A <: I1` in CHIR stage, an empty extension def still be generated in codegen stage
        SO, we have to remove `I1` from `extend A <: I1`
    */
    Utils::ProfileRecorder recorder("TranslateNominalDecls", "UpdateExtendParent");
    for (auto customDef : package->GetAllCustomTypeDef()) {
        if (!customDef->IsExtend()) {
            continue;
        }
        auto extendDef = StaticCast<ExtendDef*>(customDef);
        for (auto parent : extendDef->GetImplementedInterfaceTys()) {
            if (ParentIsOtherExtendGrandParent(*parent, *extendDef, builder)) {
                extendDef->RemoveParent(*parent);
            }
        }
    }
}

void AST2CHIR::SetExtendInfo()
{
    Utils::ProfileRecorder recorder("TranslateNominalDecls", "SetExtendInfo");
    for (auto customDef : package->GetAllCustomTypeDef()) {
        if (customDef->TestAttr(Attribute::GENERIC_INSTANTIATED)) {
            continue;
        }
        auto extendDef = DynamicCast<ExtendDef*>(customDef);
        if (extendDef == nullptr) {
            continue;
        }
        if (auto builtinType = DynamicCast<BuiltinType*>(extendDef->GetExtendedType())) {
            if (builtinType->IsCPointer()) {
                builder.GetType<CPointerType>(builder.GetUnitTy())->AddExtend(*extendDef);
            } else {
                builtinType->AddExtend(*extendDef);
            }
        } else {
            auto customType = StaticCast<CustomType*>(extendDef->GetExtendedType());
            auto customTypeDef = customType->GetCustomTypeDef();
            CJC_NULLPTR_CHECK(customTypeDef);
            CJC_ASSERT(customTypeDef->GetCustomKind() != CustomDefKind::TYPE_EXTEND);
            customTypeDef->AddExtend(*extendDef);
        }
    }
}

void AST2CHIR::SetVTable()
{
    Utils::ProfileRecorder::Start("TranslateNominalDecls", "SetVTable");
    // NOTE: Vtable should be merged after all dependent been translated.
    auto allCustomTypeDef = package->GetAllCustomTypeDef();
    auto vtableGenerator = VTableGenerator(builder);
    for (auto customDef : allCustomTypeDef) {
        if (customDef->TestAttr(Attribute::SKIP_ANALYSIS)) {
            continue;
        }
        vtableGenerator.GenerateVTable(*customDef);
    }
    Utils::ProfileRecorder::Stop("TranslateNominalDecls", "SetVTable");

    UpdateOperatorVTable(*package, builder).Update();

    Utils::ProfileRecorder::Start("TranslateNominalDecls", "SetWrapperFunc");
    bool targetIsWin = opts.target.os == Triple::OSType::WINDOWS;
    IncreKind tempKind = opts.enIncrementalCompilation ? kind : IncreKind::INVALID;
    auto wrapper = WrapVirtualFunc(builder, cachedInfo, tempKind, targetIsWin);
    for (auto customDef : allCustomTypeDef) {
        if (customDef->TestAttr(Attribute::SKIP_ANALYSIS)) {
            continue;
        }
        wrapper.CheckAndWrap(*customDef);
    }
    curVirtFuncWrapDep = wrapper.GetCurVirtFuncWrapDep();
    delVirtFuncWrapForIncr = wrapper.GetDelVirtFuncWrapForIncr();
    for (auto customDef : allCustomTypeDef) {
        if (customDef->TestAttr(Attribute::SKIP_ANALYSIS)) {
            continue;
        }
        Translator::WrapMutFunc(builder, *customDef);
    }
    Utils::ProfileRecorder::Stop("TranslateNominalDecls", "SetWrapperFunc");
}

void AST2CHIR::TranslateNominalDecls(const AST::Package& pkg)
{
    Utils::ProfileRecorder recorder("TranslateAllDecls", "TranslateNominalDecls");
    Utils::ProfileRecorder::Start("TranslateNominalDecls", "SetGenericFuncMap");
    auto trans = CreateTranslator();
    /** for AST, we can't get instantiated func decl in Nominal Decl
     *  class A {
     *      func foo<T>(a: T) {}
     *  }
     *  if `foo` is instantiated with `Bool`, we can't get `foo(a: Bool)` from `ClassDecl A`
     *  `foo(a: Bool)` is only stored in `pkg.genericInstantiatedDecls`
     *  AST need to guarantee that there is only one member func in `class A` which is matched with src code
     */
    std::unordered_map<const AST::FuncDecl*, std::vector<AST::FuncDecl*>> genericFuncMap;
    for (auto& decl : pkg.genericInstantiatedDecls) {
        // only care about member func decl
        if (decl->astKind != AST::ASTKind::FUNC_DECL || decl->outerDecl == nullptr) {
            continue;
        }
        auto funcDecl = StaticCast<AST::FuncDecl*>(decl.get());
        auto genericFunc = StaticCast<AST::FuncDecl*>(funcDecl->genericDecl);
        genericFuncMap[genericFunc].emplace_back(funcDecl);
        CJC_ASSERT(funcDecl->funcBody && !funcDecl->funcBody->paramLists.empty());
        CJC_ASSERT(genericFunc->funcBody && !genericFunc->funcBody->paramLists.empty());
        auto& funcParams = funcDecl->funcBody->paramLists[0]->params;
        auto& genericParams = genericFunc->funcBody->paramLists[0]->params;
        CJC_ASSERT(funcParams.size() == genericParams.size());
        for (size_t i = 0; i < funcParams.size(); ++i) {
            if (!funcParams[i]->desugarDecl || funcParams[i]->desugarDecl->astKind != AST::ASTKind::FUNC_DECL) {
                continue;
            }
            genericFuncMap[StaticCast<AST::FuncDecl>(genericParams[i]->desugarDecl.get())].emplace_back(
                StaticCast<AST::FuncDecl*>(funcParams[i]->desugarDecl.get()));
        }
    }
    trans.SetGenericFuncMap(genericFuncMap);
    Utils::ProfileRecorder::Stop("TranslateNominalDecls", "SetGenericFuncMap");

    Utils::ProfileRecorder::Start("TranslateNominalDecls", "TranslateDecls");
    for (auto decl : importedNominalDecls) {
        Translator::TranslateASTNode(*decl, trans);
    }
    for (auto decl : importedGenericInstantiatedNominalDecls) {
        Translator::TranslateASTNode(*decl, trans);
    }
    for (auto decl : nominalDecls) {
        Translator::TranslateASTNode(*decl, trans);
    }
    for (auto decl : genericNominalDecls) {
        Translator::TranslateASTNode(*decl, trans);
    }
    Utils::ProfileRecorder::Stop("TranslateNominalDecls", "TranslateDecls");

    SetExtendInfo();
    UpdateExtendParent();
    SetVTable();
}
} // namespace Cangjie::CHIR
