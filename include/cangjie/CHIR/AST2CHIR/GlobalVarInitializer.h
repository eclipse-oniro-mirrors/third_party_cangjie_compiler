// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_GLOBALVARINITIALIZER_H
#define CANGJIE_CHIR_GLOBALVARINITIALIZER_H

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/Utils.h"

namespace Cangjie::CHIR {
//                           ordered file                 ordered var decl
using OrderedDecl = std::pair<std::vector<Ptr<AST::File>>, std::vector<Ptr<AST::Decl>>>;

class GlobalVarInitializer {
public:
    explicit GlobalVarInitializer(Translator& trans, const ImportManager& importManager,
        std::vector<FuncBase*>& initFuncsForConstVar, bool enableIncre)
        : builder(trans.builder),
          globalSymbolTable(trans.globalSymbolTable),
          opts(trans.opts),
          importManager(importManager),
          initFuncsForConstVar(initFuncsForConstVar),
          trans(trans),
          enableIncre(enableIncre)
    {
    }

    /**
    * @brief generate global var init function
    *
    * @param pkg AST package
    * @param initOrder ordered global var decls
    */
    void Run(const AST::Package& pkg, const InitOrder& initOrder);

private:
    FuncBase* TranslateSingleInitializer(const AST::VarDecl& decl);
    bool IsIncrementalNoChange(const AST::VarDecl& decl) const;
    Func* TranslateInitializerToFunction(const AST::VarDecl& decl);
    ImportedFunc* TranslateIncrementalNoChangeVar(const AST::VarDecl& decl);
    Ptr<Value> GetGlobalVariable(const AST::VarDecl& decl);
    template <typename T, typename... Args> Ptr<Func> CreateGVInitFunc(const T& node, Args&& ... args) const;
    Ptr<Func> TranslateFileInitializer(const AST::File& file, const std::vector<Ptr<const AST::Decl>>& decls);
    Ptr<Func> TranslateFileLiteralInitializer(const AST::File& file, const std::vector<Ptr<const AST::Decl>>& decls);
    Func* TranslateVarWithPatternInitializer(const AST::VarWithPatternDecl& decl);
    Func* TranslateWildcardPatternInitializer(const AST::VarWithPatternDecl& decl);
    Func* TranslateTupleOrEnumPatternInitializer(const AST::VarWithPatternDecl& decl);
    void FillGVInitFuncWithApplyAndExit(const std::vector<Ptr<Value>>& varInitFuncs);
    void AddImportedPackageInit(const AST::Package& curPackage, const std::string& suffix = "");
    void AddGenericInstantiatedInit();
    Ptr<Func> GeneratePackageInitBase(const AST::Package& curPackage, const std::string& suffix = "");
    void InsertAnnotationVarInit(std::vector<Ptr<Value>>& initFuncs) const;
    void CreatePackageLiteralInitFunc(const AST::Package& curPackage,
        const std::vector<Ptr<const AST::VarDecl>>& importedVars, const std::vector<Ptr<Value>>& literalInitFuncs);
    void CreatePackageInitFunc(const AST::Package& curPackage,
        [[maybe_unused]] const std::vector<Ptr<Value>>& importedVarInits, std::vector<Ptr<Value>>& initFuncs);
    bool NeedVarLiteralInitFunc(const AST::Decl& decl);
    FuncBase* TranslateVarInit(const AST::Decl& var);

private:
    CHIRBuilder& builder;
    AST2CHIRNodeMap<Value>& globalSymbolTable;
    const GlobalOptions& opts;
    const ImportManager& importManager;
    std::vector<FuncBase*>& initFuncsForConstVar;
    Translator& trans;
    bool enableIncre;
};

} // namespace Cangjie::CHIR

#endif