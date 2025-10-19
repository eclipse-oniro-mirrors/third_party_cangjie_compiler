// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the ScopeManager related classes.
 */

#ifndef CANGJIE_AST_SCOPEMANAGER_H
#define CANGJIE_AST_SCOPEMANAGER_H

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "cangjie/AST/ASTContext.h"
#include "cangjie/AST/ScopeManagerApi.h"
#include "cangjie/AST/Symbol.h"

namespace Cangjie {
/**
 * Current symbol's ASTKind.
 */
enum class SymbolKind {
    FUNC,       /**< FUNC_DECL || LAMBDA_EXPR. */
    FUNC_LIKE,  /**< FUNC_DECL || LAMBDA_EXPR || PRIMARY_CTOR || MACRO_DECL */
    STRUCT,     /**< CLASS_DECL || INTERFACE_DECL || RECORD_DECL || ENUM_DECL || EXTEND_DECL. */
    TOPLEVEL,   /**< All toplevel decls. */
};

/** Manage @c scopeName, @c scopeLevel, provide Symbol query from @c scopeName.
 */
class ScopeManager {
public:
    ScopeManager() = default;
    ~ScopeManager() = default;
    /**
     * When entering a block, we need initialize the scope, we do two things:
     * 1. Increment @p ctx.currentScopeLevel
     * 2. Modify @p ctx.currentScopeName
     */
    void InitializeScope(ASTContext& ctx);

    /**
     * Calc scope gate name of @p ctx.currentScopeName, only used in collect
     * phase.
     *
     * @return Scope gate name, when @p ctx.currentScopeName is a0ab, the scope
     * gate name maybe a0ab_c.
     */
    std::string CalcScopeGateName(const ASTContext& ctx);

    /**
     * When leave a block, we need finalize the scope, we do two things:
     * 1. Decrement @p ctx.currentScopeLevel
     * 2. Modify @p ctx.currentScopeName
     */
    void FinalizeScope(ASTContext& ctx);

    /** Get the out most symbol of given kind from the @p scopeName. */
    static AST::Symbol* GetOutMostSymbol(const ASTContext& ctx, SymbolKind symbolKind, const std::string& scopeName);

    static AST::Symbol* GetRefLoopSymbol(const ASTContext& ctx, const AST::Node& self);
    static AST::Symbol* GetCurSymbolByKind(
        const SymbolKind symbolKind, const ASTContext& ctx, const std::string& scopeName);

    /**
     * Satisfy and Fail are two predicates.
     * When Fail(sym) is true, the loop stops and returns nullptr;
     * When Satisfy(sym) is true, the loop stops and returns the satisfied sym.
     * Otherwise, the loop continues to find the desired symbol (in the outside scope)
     */
    static AST::Symbol* GetCurSatisfiedSymbol(const ASTContext& ctx, const std::string& scopeName,
        const std::function<bool(AST::Symbol&)>& satisfy, const std::function<bool(AST::Symbol&)>& fail);

    static AST::Symbol* GetCurSatisfiedSymbolUntilTopLevel(const ASTContext& ctx,
        const std::string& scopeName, const std::function<bool(AST::Symbol&)>& satisfy);

    static AST::Symbol* GetCurOuterDeclOfScopeLevelX(
        const ASTContext& ctx, const AST::Node& checkNode, uint32_t scopeLevel);

    void Reset()
    {
        charIndexes = {0};
    }

private:
    static std::string GetLayerName(int layerIndex, char split = ScopeManagerApi::scopeNameSplit);
    static unsigned GetLayerNameLength(int layerIndex);
    // a-z A-Z
    constexpr static int numChar = 52;
    constexpr static std::array<char, numChar> chars{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
        'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
    // Index is the number of the \c scopeName, start from 0.
    std::vector<int> charIndexes{0};
};
} // namespace Cangjie
#endif
