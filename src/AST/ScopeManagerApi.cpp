// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements ScopeManager apis.
 */

#include "cangjie/AST/ScopeManagerApi.h"

#include "cangjie/AST/ASTContext.h"

using namespace Cangjie;
using namespace AST;

Symbol* ScopeManagerApi::GetScopeGate(const ASTContext& ctx, const std::string& scopeName)
{
    auto it = ctx.invertedIndex.scopeGateMap.find(scopeName);
    if (it != ctx.invertedIndex.scopeGateMap.end()) {
        return it->second;
    }
    return nullptr;
}

std::string ScopeManagerApi::GetScopeGateName(const std::string& scopeName)
{
    std::string currentScope;
    auto found = scopeName.find_last_of(childScopeNameSplit);
    if (found != std::string::npos) {
        currentScope = scopeName.substr(0, found);
    } else {
        currentScope = scopeName;
    }
    found = currentScope.find_last_of(scopeNameSplit);
    if (found != std::string::npos) {
        currentScope.replace(found, 1, 1, childScopeNameSplit);
    } else {
        // Toplevel don't have root name.
        return "";
    }
    return currentScope;
}
