// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the ScopeManager related classes.
 */

#include "ScopeManager.h"

#include <cmath>

#include "cangjie/AST/ASTContext.h"
#include "cangjie/AST/ScopeManagerApi.h"
#include "cangjie/AST/Symbol.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Utils/Utils.h"
#include "cangjie/AST/ASTCasting.h"

using namespace Cangjie;
using namespace AST;
using namespace std::placeholders;

namespace {
bool IsRefLoop(const Symbol& sym, const Node& self)
{
    Ptr<Expr> condExpr = nullptr;
    Ptr<Expr> guardExpr = nullptr;
    // As for while (true) { while (break) {...} }, the break binds to the outer while loop.
    if (sym.node->astKind == ASTKind::WHILE_EXPR) {
        condExpr = RawStaticCast<WhileExpr*>(sym.node)->condExpr.get();
    } else if (sym.node->astKind == ASTKind::DO_WHILE_EXPR) {
        condExpr = RawStaticCast<DoWhileExpr*>(sym.node)->condExpr.get();
    } else if (sym.node->astKind == ASTKind::FOR_IN_EXPR) {
        condExpr = RawStaticCast<ForInExpr*>(sym.node)->inExpression.get();
        guardExpr = RawStaticCast<ForInExpr*>(sym.node)->patternGuard.get();
    } else {
        return sym.node->IsLoopExpr();
    }

    bool isInLoopCond = false;
    auto markIsInLoopCond = [&isInLoopCond, &self](Ptr<const Node> node) -> VisitAction {
        switch (node->astKind) {
            case ASTKind::JUMP_EXPR:
                if (&self == node) {
                    isInLoopCond = true;
                }
                return VisitAction::SKIP_CHILDREN;
            case ASTKind::WHILE_EXPR:
            case ASTKind::DO_WHILE_EXPR:
            case ASTKind::FOR_IN_EXPR:
            case ASTKind::FUNC_DECL:
            case ASTKind::LAMBDA_EXPR:
                return VisitAction::SKIP_CHILDREN;
            default:
                return VisitAction::WALK_CHILDREN;
        }
    };
    Walker w(condExpr, nullptr, markIsInLoopCond);
    w.Walk();
    if (guardExpr) {
        Walker w2(guardExpr, nullptr, markIsInLoopCond);
        w2.Walk();
    }
    return !isInLoopCond;
}
} // namespace

AST::Symbol* ScopeManager::GetRefLoopSymbol(const ASTContext& ctx, const Node& self)
{
    return GetCurSatisfiedSymbol(ctx, self.scopeName, [&self](auto& sym) { return IsRefLoop(sym, self); },
        [](auto& sym) { return sym.node->astKind == ASTKind::FUNC_BODY; });
}

void ScopeManager::InitializeScope(ASTContext& ctx)
{
    ctx.currentScopeLevel += 1;
    if (ctx.currentScopeLevel > ctx.currentMaxDepth) {
        ctx.currentMaxDepth = ctx.currentScopeLevel;
    }
    // Init scope level is 0, but size of (char indexes: {0}) is 1
    // so we need charIndexes.size() - 1 to keep them in the same start line.
    if (ctx.currentScopeLevel > charIndexes.size() - 1) {
        // Add a new and first scope.
        charIndexes.push_back(0);
        ctx.currentScopeName.push_back(ScopeManagerApi::scopeNameSplit);
        ctx.currentScopeName.push_back('a');
    } else {
        // Increment index.
        charIndexes[ctx.currentScopeLevel] += 1;
        if (ctx.currentScopeLevel > 1 && charIndexes[ctx.currentScopeLevel] == 0) {
            // When currentScopeLevel > 1, we check whether the index has been cleared to -1(according to
            // FinalizeScope), we need to restart encoding from a.
            ctx.currentScopeName = ctx.currentScopeName + ScopeManagerApi::scopeNameSplit + 'a';
        } else {
            ctx.currentScopeName += GetLayerName(charIndexes[ctx.currentScopeLevel]);
        }
    }
    ctx.invertedIndex.scopeNameTrie->Insert(ctx.currentScopeName);
}

std::string ScopeManager::CalcScopeGateName(const ASTContext& ctx)
{
    unsigned scopeLevel = ctx.currentScopeLevel + 1;
    if (scopeLevel > charIndexes.size() - 1) {
        return ctx.currentScopeName + ScopeManagerApi::childScopeNameSplit + 'a';
    }
    if (scopeLevel > 1 && charIndexes[scopeLevel] == -1) {
        // When scopeLevel > 1, we check whether the index has been cleared to -1(according to
        // FinalizeScope), we need to restart encoding from a.
        return ctx.currentScopeName + ScopeManagerApi::childScopeNameSplit + 'a';
    }
    return ctx.currentScopeName + GetLayerName(charIndexes[scopeLevel] + 1, ScopeManagerApi::childScopeNameSplit);
}

Symbol* ScopeManager::GetOutMostSymbol(const ASTContext& ctx, SymbolKind symbolKind, const std::string& scopeName)
{
    Symbol* curFuncSym = GetCurSymbolByKind(symbolKind, ctx, scopeName);
    Symbol* insideFuncSym = curFuncSym;
    while (curFuncSym && curFuncSym->scopeName.length() > 0) {
        insideFuncSym = curFuncSym;
        curFuncSym = GetCurSymbolByKind(symbolKind, ctx, curFuncSym->scopeName);
    }
    return insideFuncSym;
}

Symbol* ScopeManager::GetCurSatisfiedSymbol(const ASTContext& ctx, const std::string& scopeName,
    const std::function<bool(AST::Symbol&)>& satisfy, const std::function<bool(AST::Symbol&)>& fail)
{
    std::string scopeGateName = ScopeManagerApi::GetScopeGateName(scopeName);
    while (true) {
        auto sym = ScopeManagerApi::GetScopeGate(ctx, scopeGateName);
        std::string tmpScopeName;
        if (sym) {
            tmpScopeName = sym->scopeName;
            if (fail(*sym)) {
                return nullptr;
            }
            if (satisfy(*sym)) {
                return sym;
            }
        } else {
            if (scopeGateName.empty()) {
                // Reach top but not found.
                return nullptr;
            }
            tmpScopeName = ScopeManagerApi::GetParentScopeName(scopeGateName);
        }
        scopeGateName = ScopeManagerApi::GetScopeGateName(tmpScopeName);
    }
}

Symbol* ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(const ASTContext& ctx,
    const std::string& scopeName, const std::function<bool(AST::Symbol&)>& satisfy)
{
    return GetCurSatisfiedSymbol(ctx, scopeName, satisfy, [](const Symbol& /* sym */) { return false; });
}

Symbol* ScopeManager::GetCurOuterDeclOfScopeLevelX(
    const ASTContext& ctx, const Node& checkNode, uint32_t scopeLevel)
{
    if (checkNode.scopeLevel < scopeLevel) {
        return nullptr;
    }
    std::string scopeGateName = ScopeManagerApi::GetScopeGateName(checkNode.scopeName);
    while (true) {
        auto sym = ScopeManagerApi::GetScopeGate(ctx, scopeGateName);
        if (!sym) {
            return nullptr;
        }
        if (sym->scopeLevel == scopeLevel) {
            return sym;
        }
        scopeGateName = ScopeManagerApi::GetScopeGateName(sym->scopeName);
    }
}

void ScopeManager::FinalizeScope(ASTContext& ctx)
{
    // Calc length of last layer's name in whole scope, 1 is SPLIT length.
    unsigned backLength = GetLayerNameLength(charIndexes[ctx.currentScopeLevel]) + 1;
    ctx.currentScopeName.erase(ctx.currentScopeName.size() - backLength);
    ctx.currentScopeLevel -= 1;
    // Reset finalized scope char indexes when finish a toplevel decl.
    if (ctx.currentScopeLevel == 0) {
        // Clear from scope level 2.
        // func a {
        //   // scope_name: a0a && scope_level: 1
        //   func aa {
        //     // scope_name: a0a0a && scope_level: 2
        //   }
        // }
        //
        // func b {
        //   // scope_name: a0b && scope_level: 1
        //   func ba {
        //     // scope_name: a0b0a && scope_level: 2
        //   }
        // }
        // Scope in aa and ba will be cleared.
        for (size_t i = 2; i <= ctx.currentMaxDepth; i++) {
            charIndexes[i] = -1;
        }
    }
}

// Get layer name of current index with given separator.
std::string ScopeManager::GetLayerName(int layerIndex, char split)
{
    if (layerIndex == 0) {
        return TOPLEVEL_SCOPE_NAME;
    }
    unsigned length = GetLayerNameLength(layerIndex);
    std::vector<unsigned> y(length + 1);
    std::vector<unsigned> n(length + 1);
    n[0] = static_cast<unsigned>(layerIndex);
    for (unsigned i = 1; i <= length; ++i) {
        y[i - 1] = n[i - 1] % numChar;
        n[i] = n[i - 1] / numChar;
    }
    std::string s;
    for (unsigned j = 0; j < length; ++j) {
        s += chars[y[j]];
    }
    std::reverse(s.begin(), s.end());
    return split + s;
}

// Calculate number of char which is needed to present given layer's name.
// Base is 52 (number of capital and small letter).
unsigned ScopeManager::GetLayerNameLength(int layerIndex)
{
    if (layerIndex == 0) {
        return 1;
    }
    return static_cast<unsigned>(floor(log(layerIndex) / log(numChar))) + 1;
}

Symbol* ScopeManager::GetCurSymbolByKind(
    const SymbolKind symbolKind, const ASTContext& ctx, const std::string& scopeName)
{
    std::function<bool(const Symbol& sym)> finder;
    switch (symbolKind) {
        case SymbolKind::STRUCT:
            finder = [](const Symbol& sym) { return sym.node->IsNominalDecl(); };
            break;
        case SymbolKind::FUNC:
            finder = [](const Symbol& sym) { return sym.node->IsFunc(); };
            break;
        case SymbolKind::FUNC_LIKE:
            finder = [](const Symbol& sym) { return sym.node->IsFuncLike(); };
            break;
        case SymbolKind::TOPLEVEL:
            finder = [](const Symbol& sym) { return sym.scopeLevel == 0; };
            break;
    }
    return GetCurSatisfiedSymbolUntilTopLevel(ctx, scopeName, finder);
}
