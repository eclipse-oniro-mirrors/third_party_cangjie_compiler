// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares provide API to get the checking rules of modifiers.
 */

#ifndef CANGJIE_PARSE_PARSE_MODIFIERS_RULES_H
#define CANGJIE_PARSE_PARSE_MODIFIERS_RULES_H

#include "Parser.h"

namespace Cangjie {
using ModifierRules = std::unordered_map<ScopeKind, std::unordered_map<TokenKind, std::vector<TokenKind>>>;
const ModifierRules& GetModifierRulesByDefKind(DefKind defKind);
const ModifierRules& GetModifierWarningRulesByDefKind(DefKind defKind);
std::optional<AST::Attribute> GetAttributeByModifier(TokenKind tokenKind);
} // namespace Cangjie
#endif // CANGJIE_PARSE_PARSEMODIFIERS_H
