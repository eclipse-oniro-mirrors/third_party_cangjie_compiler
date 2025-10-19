// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Desugar/AfterTypeCheck.h"

#include "cangjie/AST/ASTCasting.h"

using namespace Cangjie;
using namespace AST;

namespace {
void RemoveSubScriptTypeLineInfo(const Expr& assignmentExpr)
{
    if (assignmentExpr.desugarExpr == nullptr ||
        assignmentExpr.desugarExpr->astKind != Cangjie::AST::ASTKind::CALL_EXPR ||
        RawStaticCast<CallExpr*>(assignmentExpr.desugarExpr.get())->args.empty()) {
        return;
    }
    auto arrayCallExpr = RawStaticCast<CallExpr*>(assignmentExpr.desugarExpr.get());
    assignmentExpr.desugarExpr->begin.Mark(PositionStatus::IGNORE);
    auto arrayArg = arrayCallExpr->args[0].get();
    bool isRefLiteral = Utils::In(arrayArg->expr->astKind,
        {AST::ASTKind::LIT_CONST_EXPR, AST::ASTKind::REF_EXPR, AST::ASTKind::ARRAY_LIT, AST::ASTKind::TUPLE_LIT,
            AST::ASTKind::SUBSCRIPT_EXPR});
    if (isRefLiteral) {
        arrayArg->expr->begin.Mark(PositionStatus::IGNORE);
        arrayArg->begin.Mark(PositionStatus::IGNORE);
        if (arrayArg->expr->astKind == AST::ASTKind::SUBSCRIPT_EXPR) {
            RemoveSubScriptTypeLineInfo(*arrayArg->expr);
        }
    }
}

void CleanExprLineInfo(Expr& expr);

void CleanCallExprLineInfo(CallExpr& ce)
{
    bool isEnumCtor = ce.resolvedFunction && ce.resolvedFunction->TestAttr(Attribute::ENUM_CONSTRUCTOR);
    auto bid = DynamicCast<BuiltInDecl*>(ce.baseFunc ? ce.baseFunc->GetTarget() : nullptr);
    bool isCStrCtor = bid && bid->type == BuiltInType::CSTRING;
    if (isCStrCtor) {
        ce.begin.Mark(PositionStatus::IGNORE);
    }
    if (isEnumCtor || isCStrCtor) {
        for (auto& arg : ce.args) {
            CleanExprLineInfo(*arg->expr);
        }
    }
}

void CleanExprLineInfo(Expr& expr)
{
    bool isRefLiteral =
        Utils::In(expr.astKind, {AST::ASTKind::LIT_CONST_EXPR, AST::ASTKind::REF_EXPR, AST::ASTKind::RANGE_EXPR});
    bool isArrayLit = expr.astKind == AST::ASTKind::ARRAY_LIT;
    bool isTupleLit = expr.astKind == AST::ASTKind::TUPLE_LIT;
    bool isUnsafeBlock = expr.astKind == ASTKind::BLOCK && expr.TestAttr(Attribute::UNSAFE);

    if (isRefLiteral) {
        expr.begin.Mark(PositionStatus::IGNORE);
        if (expr.desugarExpr) {
            expr.desugarExpr->begin.Mark(PositionStatus::IGNORE);
        }
    } else if (isArrayLit) {
        auto& arrayExpr = static_cast<ArrayLit&>(expr);
        expr.begin.Mark(PositionStatus::IGNORE);
        for (auto& child : arrayExpr.children) {
            CleanExprLineInfo(*child.get());
        }
    } else if (isTupleLit) {
        auto& tupleExpr = static_cast<TupleLit&>(expr);
        expr.begin.Mark(PositionStatus::IGNORE);
        for (auto& child : tupleExpr.children) {
            CleanExprLineInfo(*child.get());
        }
    } else if (isUnsafeBlock) {
        auto& blk = static_cast<Block&>(expr);
        expr.begin.Mark(PositionStatus::IGNORE);
        blk.unsafePos.Mark(PositionStatus::IGNORE);
        for (auto& body : blk.body) {
            if (auto be = DynamicCast<Expr*>(body.get()); be) {
                CleanExprLineInfo(*be);
            }
        }
    } else if (auto ce = DynamicCast<CallExpr*>(&expr)) {
        CleanCallExprLineInfo(*ce);
    } else if (expr.astKind == AST::ASTKind::SUBSCRIPT_EXPR && expr.desugarExpr) {
        // if an expr is subscript like array[0], remove the line number of index.
        // the type of index should be in the filter set, too.
        expr.begin.Mark(PositionStatus::IGNORE);
        expr.desugarExpr->begin.Mark(PositionStatus::IGNORE);
        if (expr.desugarExpr->astKind != AST::ASTKind::CALL_EXPR) {
            return;
        }
        auto refExpr = RawStaticCast<CallExpr*>(expr.desugarExpr.get());
        if (refExpr->baseFunc->astKind != AST::ASTKind::MEMBER_ACCESS) {
            return;
        }
        auto ma = RawStaticCast<MemberAccess*>(refExpr->baseFunc.get());
        if (!ma->baseExpr->ty->IsStructArray()) {
            return;
        }
        ma->baseExpr->begin.Mark(PositionStatus::IGNORE);
        RemoveSubScriptTypeLineInfo(expr);
    }
}

void ProcessDefaultParamLineInfo(FuncDecl& fd)
{
    // Every default parameter only have one element in their funcBody.
    if (!fd.funcBody || !fd.funcBody->body || fd.funcBody->body->body.empty() ||
        fd.funcBody->body->body[0]->astKind != AST::ASTKind::RETURN_EXPR) {
        return;
    }
    auto paramAssignment = RawStaticCast<ReturnExpr*>(fd.funcBody->body->body[0].get());
    bool isRefLiteral = Utils::In(paramAssignment->expr->astKind,
        {AST::ASTKind::LIT_CONST_EXPR, AST::ASTKind::REF_EXPR, AST::ASTKind::ARRAY_LIT, AST::ASTKind::TUPLE_LIT,
            AST::ASTKind::SUBSCRIPT_EXPR, AST::ASTKind::RANGE_EXPR});
    bool isUnsafeBlock =
        paramAssignment->expr->astKind == ASTKind::BLOCK && paramAssignment->expr->TestAttr(Attribute::UNSAFE);
    if (isRefLiteral || isUnsafeBlock) {
        paramAssignment->begin.Mark(PositionStatus::IGNORE);
    }
    fd.begin.Mark(PositionStatus::IGNORE);
    CleanExprLineInfo(*paramAssignment->expr);
}
} // namespace

namespace Cangjie::Sema::Desugar::AfterTypeCheck {
/**
 * For the default parameter, if assigned by a LIT_CONST_EXPR type, Ref type, or Subscript type.
 * which means it does not need line number for debug.
 * So, remove the assignment line number from desugared function.
 */
void PostProcessFuncParam(const FuncParam& fp, const GlobalOptions& options)
{
    if (!fp.desugarDecl || !fp.desugarDecl->funcBody->body) {
        return;
    }
    if (!options.enableCoverage) {
        ProcessDefaultParamLineInfo(*fp.desugarDecl);
    }
}
} // namespace Cangjie::Sema::Desugar::AfterTypeCheck
