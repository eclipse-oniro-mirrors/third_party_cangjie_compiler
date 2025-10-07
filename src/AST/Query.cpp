// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements the Query and related classes.
 */

#include "cangjie/AST/Query.h"

#include <memory>
#include <optional>

#include "QueryParser.h"

using namespace Cangjie;

namespace {
std::string OpToStr(Operator op)
{
    switch (op) {
        case Operator::AND:
            return "&&";
        case Operator::NOT:
            return "!";
        case Operator::OR:
            return "||";
        default:
            return std::to_string(static_cast<int>(op));
    }
}
} // namespace

std::unique_ptr<Query> QueryParser::Parse()
{
    return ParseBooleanClause();
}

std::unique_ptr<Query> QueryParser::ParseBooleanClause()
{
    std::unique_ptr<Query> left;
    if (Skip(TokenKind::LPAREN)) {
        left = ParseParenClause();
    } else if (Seeing(TokenKind::IDENTIFIER) || Seeing(TokenKind::WILDCARD)) {
        left = ParseTerm();
    } else {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_expected_query_symbol);
        return nullptr;
    }
    if (!left) {
        return nullptr;
    }
    bool unexpectedEnd = !SeeingAny({TokenKind::AND, TokenKind::OR, TokenKind::NOT, TokenKind::END}) &&
        !(Seeing(TokenKind::RPAREN) && parsingParenClause);
    if (unexpectedEnd) {
        Next();
        GetDiagnosticEngine().Diagnose(
            LookAhead().Begin(), DiagKind::parse_query_expected_logic_symbol, LookAhead().Value());
        return nullptr;
    }
    while (SeeingAny({TokenKind::AND, TokenKind::OR, TokenKind::NOT})) {
        auto op = std::make_unique<Query>();
        op->type = QueryType::OP;
        // Determine the op.
        if (Skip(TokenKind::AND)) {
            op->op = Operator::AND;
        } else if (Skip(TokenKind::OR)) {
            op->op = Operator::OR;
        } else if (Skip(TokenKind::NOT)) {
            op->op = Operator::NOT;
        }

        std::unique_ptr<Query> right;
        if (Skip(TokenKind::LPAREN)) {
            right = ParseParenClause();
        } else if (Seeing(TokenKind::IDENTIFIER) || Seeing(TokenKind::WILDCARD)) {
            right = ParseTerm();
        } else {
            GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_expected_query_symbol);
            return nullptr;
        }

        if (!right) {
            return nullptr;
        }

        op->right = std::move(right);
        op->left = std::move(left);
        left = std::move(op);
    }
    return left;
}

std::unique_ptr<Query> QueryParser::ParseParenClause()
{
    parsingParenClause = true;
    std::unique_ptr<Query> curNode = ParseBooleanClause();
    parsingParenClause = false;
    if (!Skip(TokenKind::RPAREN)) {
        GetDiagnosticEngine().DiagnoseRefactor(
            DiagKindRefactor::parse_expected_character, LookAhead().Begin(), ")", LookAhead().Value());
        return nullptr;
    }
    return curNode;
}

std::optional<std::string> QueryParser::ParseComparator()
{
    if (Skip(TokenKind::ASSIGN)) {
        return "=";
    } else if (Skip(TokenKind::LT)) {
        return "<";
    } else if (Skip(TokenKind::LE)) {
        return "<=";
    } else if (SeeingCombinator({TokenKind::GT, TokenKind::ASSIGN})) {
        SkipCombinator({TokenKind::GT, TokenKind::ASSIGN});
        return ">=";
    } else if (Skip(TokenKind::GT)) {
        return ">";
    } else {
        return std::nullopt;
    }
}

std::unique_ptr<Query> QueryParser::ParsePositionTerm()
{
    auto getNumber = [](const std::string& str) -> int {
        try {
            return std::stoi(str.c_str());
        } catch (...) { // If parse failed, just return 0 for any exception.
            return 0;
        }
    };
    auto pos = std::make_unique<Query>();
    pos->key = TOKEN_KIND_VALUES[static_cast<int>(TokenKind::WILDCARD)];
    pos->type = QueryType::POS;
    auto sign = ParseComparator();
    if (!sign.has_value()) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_expected_position_compare_operator);
        return nullptr;
    }
    pos->sign = *sign;
    if (!Skip(TokenKind::LPAREN)) {
        GetDiagnosticEngine().DiagnoseRefactor(
            DiagKindRefactor::parse_expected_character, LookAhead().Begin(), "(", LookAhead().Value());
        return nullptr;
    }
    if (!Skip(TokenKind::INTEGER_LITERAL)) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_position_illegal_file_id);
        return nullptr;
    }
    pos->pos.fileID = static_cast<unsigned int>(getNumber(LookAhead().Value()));
    if (!Skip(TokenKind::COMMA)) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_position_comma_required);
        return nullptr;
    }
    if (!Skip(TokenKind::INTEGER_LITERAL)) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_position_illegal_line_num);
        return nullptr;
    }
    pos->pos.line = getNumber(LookAhead().Value());
    if (!Skip(TokenKind::COMMA)) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_position_comma_required);
        return nullptr;
    }
    if (!Skip(TokenKind::INTEGER_LITERAL)) {
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_position_illegal_column_num);
        return nullptr;
    }
    pos->pos.column = getNumber(LookAhead().Value());
    if (!Skip(TokenKind::RPAREN)) {
        GetDiagnosticEngine().DiagnoseRefactor(
            DiagKindRefactor::parse_expected_character, LookAhead().Begin(), ")", LookAhead().Value());
        return nullptr;
    }
    return pos;
}

std::unique_ptr<Query> QueryParser::ParseNormalTerm()
{
    auto term = std::make_unique<Query>();
    if (Skip(TokenKind::IDENTIFIER)) {
        term->key = LookAhead().Value();
    }
    if (!Skip(TokenKind::COLON)) {
        GetDiagnosticEngine().DiagnoseRefactor(
            DiagKindRefactor::parse_expected_character, LookAhead().Begin(), ":", LookAhead().Value());
        return nullptr;
    }
    term->sign = "=";
    // Suffix query, `*decl`.
    if (Skip(TokenKind::MUL)) {
        term->type = QueryType::STRING;
        std::string prefix;
        if (Skip(TokenKind::WILDCARD)) {
            prefix = "_";
        }
        if (Seeing(TokenKind::INT8, TokenKind::RUNE_LITERAL)) {
            term->value = prefix + LookAhead().Value();
            term->matchKind = MatchKind::SUFFIX;
            Next();
        } else {
            GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_invalid_query_value);
            return nullptr;
        }
    } else if (Seeing(TokenKind::INT8, TokenKind::RUNE_LITERAL) && !Seeing(TokenKind::DOLLAR)) {
        term->value = LookAhead().Value();
        term->type = QueryType::STRING;
        Next();
        // Prefix query, `foo*`.
        if (Skip(TokenKind::MUL)) {
            term->matchKind = MatchKind::PREFIX;
        }
    } else {
        if (Seeing(TokenKind::DOLLAR_IDENTIFIER) || Seeing(TokenKind::DOLLAR)) {
            GetDiagnosticEngine().DiagnoseRefactor(DiagKindRefactor::lex_unrecognized_symbol, LookAhead(), "$");
        }
        GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_invalid_query_value);
        return nullptr;
    }
    return term;
}

std::unique_ptr<Query> QueryParser::ParseTerm()
{
    if (Seeing(TokenKind::IDENTIFIER)) {
        return ParseNormalTerm();
    }
    if (Skip(TokenKind::WILDCARD)) {
        return ParsePositionTerm();
    }
    GetDiagnosticEngine().Diagnose(LookAhead().Begin(), DiagKind::parse_query_expected_query_symbol);
    return nullptr;
}

void Query::PrettyPrint(std::string& result) const
{
    if (type == QueryType::OP) {
        result.append("(");
        left->PrettyPrint(result);
        result.append(OpToStr(op));
        right->PrettyPrint(result);
        result.append(")");
    } else if (type == QueryType::POS) {
        result.append(key).append(sign).append("(");
        result.append(pos.ToString());
        result.append(")");
    } else {
        if (matchKind == MatchKind::PREFIX) {
            result.append(key).append(sign).append(value + "*");
        } else if (matchKind == MatchKind::SUFFIX) {
            result.append(key).append(sign).append("*" + value);
        } else {
            result.append(key).append(sign).append(value);
        }
    }
}
