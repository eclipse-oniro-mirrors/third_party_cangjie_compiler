// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "TypeCheckerImpl.h"

#include "BuiltInOperatorUtil.h"
#include "Desugar/DesugarInTypeCheck.h"
#include "Diags.h"
#include "TypeCheckUtil.h"

#include "cangjie/AST/RecoverDesugar.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include "DiagSuppressor.h"

using namespace Cangjie;
using namespace Sema;
using namespace TypeCheckUtil;

namespace {
Expr& GetNonParenExpr(ParenExpr& pe)
{
    Ptr<ParenExpr> parent = &pe;
    CJC_NULLPTR_CHECK(parent->expr);
    while (parent->expr->astKind == ASTKind::PAREN_EXPR) {
        parent = StaticCast<ParenExpr*>(parent->expr.get());
        CJC_NULLPTR_CHECK(parent->expr);
    }
    return *parent->expr;
}

UnaryExpr& GetLeafUnaryExpr(UnaryExpr& ue)
{
    Ptr<UnaryExpr> leaf = &ue;
    while (true) {
        CJC_NULLPTR_CHECK(leaf->expr);
        // If a `UnaryExpr` has `desugarExpr`, it is guaranteed to be correct and cannot be the leaf.
        if (leaf->expr->astKind == ASTKind::UNARY_EXPR && leaf->expr->desugarExpr == nullptr) {
            leaf = StaticCast<UnaryExpr*>(leaf->expr.get());
        } else if (leaf->expr->astKind == ASTKind::PAREN_EXPR) {
            Expr& child = GetNonParenExpr(StaticCast<ParenExpr&>(*leaf->expr));
            if (child.astKind == ASTKind::UNARY_EXPR && child.desugarExpr == nullptr) {
                leaf = StaticCast<UnaryExpr*>(&child);
            } else {
                break;
            }
        } else {
            break;
        }
        CJC_NULLPTR_CHECK(leaf);
    }
    return *leaf;
}
} // namespace

void TypeChecker::TypeCheckerImpl::DiagnoseForUnaryExprWithTarget(ASTContext& ctx, UnaryExpr& ue, Ty& target)
{
    // `leaf` is responsible for the invalid type.
    UnaryExpr& leaf = GetLeafUnaryExpr(ue);
    if (Ty::IsTyCorrect(Synthesize(ctx, leaf.expr.get())) && ReplaceIdealTy(*leaf.expr)) {
        DiagInvalidUnaryExprWithTarget(diag, leaf, target);
    }
}

void TypeChecker::TypeCheckerImpl::DiagnoseForUnaryExpr(ASTContext& ctx, UnaryExpr& ue)
{
    // `leaf` is responsible for the invalid type.
    UnaryExpr& leaf = GetLeafUnaryExpr(ue);
    if (Ty::IsTyCorrect(Synthesize(ctx, leaf.expr.get())) && ReplaceIdealTy(*leaf.expr)) {
        DiagInvalidUnaryExpr(diag, leaf);
    }
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynUnaryExpr(ASTContext& ctx, UnaryExpr& ue)
{
    if (ue.desugarExpr) {
        return ue.desugarExpr->ty;
    }
    if (!Ty::IsTyCorrect(Synthesize(ctx, ue.expr.get()))) {
        ue.ty = TypeManager::GetInvalidTy();
        return ue.ty;
    }
    if (Ty::IsTyCorrect(SynBuiltinUnaryExpr(ctx, ue))) {
        ReplaceIdealTy(*ue.expr);
        ue.ty = ue.expr->ty;
        return ue.ty;
    }

    DesugarOperatorOverloadExpr(ctx, ue);
    if (Ty::IsTyCorrect(Synthesize(ctx, ue.desugarExpr.get()))) {
        ue.ty = ue.desugarExpr->ty;
        ReplaceTarget(&ue, StaticCast<CallExpr*>(ue.desugarExpr.get())->resolvedFunction);
    } else {
        RecoverToUnaryExpr(ue);
        DiagnoseForUnaryExpr(ctx, ue);
        ue.ty = TypeManager::GetInvalidTy();
    }
    return ue.ty;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynBuiltinUnaryExpr(ASTContext& ctx, UnaryExpr& ue)
{
    auto ty = Synthesize(ctx, ue.expr.get());
    if (!Ty::IsTyCorrect(ty)) {
        return TypeManager::GetInvalidTy();
    }
    // Check if type is available for the given operator.
    if (!IsUnaryOperator(ue.op)) {
        return TypeManager::GetInvalidTy();
    }
    const auto& typeCandidate = GetUnaryOpTypeCandidates(ue.op);
    if (auto tv = DynamicCast<TyVar*>(ty); tv && tv->isPlaceholder) {
        switch (PickConstaintFromTys(*tv, TypeMapToTys(typeCandidate, true), true)) {
            case MatchResult::UNIQUE:
                ue.ty = typeManager.TryGreedySubst(tv);
                return ue.ty;
            case MatchResult::AMBIGUOUS:
            case MatchResult::NONE:
                return TypeManager::GetInvalidTy();
        }
    }
    for (auto& type : typeCandidate) {
        auto primitiveTy = TypeManager::GetPrimitiveTy(type.second);
        if (typeManager.IsSubtype(ty, primitiveTy)) {
            ue.ty = ty;
            return ty;
        }
    }
    return TypeManager::GetInvalidTy();
}

bool TypeChecker::TypeCheckerImpl::ChkUnaryExpr(ASTContext& ctx, Ty& target, UnaryExpr& ue)
{
    if (ue.desugarExpr) {
        return typeManager.IsSubtype(ue.desugarExpr->ty, &target);
    }
    bool isWellTyped = true;
    // If the 'target' is correct type, 'unboxedTy' must also be correct;
    auto unboxedTy = TypeCheckUtil::UnboxOptionType(&target);
    Ptr<Ty> retTy = nullptr;
    // If 'unboxedTy' is builtin type, we need to use this type checking with expression for ideal literal,
    // otherwise only only check type relation here for possible type boxing relation.
    if (unboxedTy->IsBuiltin()) {
        DiagSuppressor ds(diag); // report error later
        isWellTyped = Check(ctx, unboxedTy, ue.expr.get());
        ue.expr->ty = typeManager.TryGreedySubst(ue.expr->ty);
        retTy = SynBuiltinUnaryExpr(ctx, ue);
        isWellTyped = isWellTyped && Ty::IsTyCorrect(retTy) && Ty::IsTyCorrect(&target);
    } else {
        retTy = SynBuiltinUnaryExpr(ctx, ue);
        isWellTyped = Ty::IsTyCorrect(retTy) && Ty::IsTyCorrect(&target) && typeManager.IsSubtype(retTy, &target);
    }

    if (isWellTyped) { // If this is a built-in unary expr.
        ReplaceIdealTy(*ue.expr);
        ue.ty = ue.expr->ty;
        return true;
    }
    // Try operator overload.
    DesugarOperatorOverloadExpr(ctx, ue);
    if (Check(ctx, &target, ue.desugarExpr.get())) {
        ue.ty = ue.desugarExpr->ty;
        ReplaceTarget(&ue, StaticCast<CallExpr*>(ue.desugarExpr.get())->resolvedFunction);
        return true;
    }
    auto synTy = Synthesize(ctx, ue.desugarExpr.get());
    RecoverToUnaryExpr(ue);
    ctx.DeleteDesugarExpr(ue.desugarExpr);
    // Report errors.
    typeManager.ReplaceIdealTy(&retTy);
    bool retTyMismatch = isWellTyped || (Ty::IsTyCorrect(&target) && Ty::IsTyCorrect(synTy));
    if (retTyMismatch) {
        DiagMismatchedTypesWithFoundTy(diag, ue, target, isWellTyped ? *retTy : *synTy);
    } else if (Ty::IsTyCorrect(&target)) {
        DiagnoseForUnaryExprWithTarget(ctx, ue, target);
    } else {
        DiagnoseForUnaryExpr(ctx, ue);
    }
    ue.ty = TypeManager::GetInvalidTy();
    return false;
}
