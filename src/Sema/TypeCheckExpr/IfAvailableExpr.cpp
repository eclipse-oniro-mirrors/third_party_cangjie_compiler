// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Diags.h"
#include "TypeCheckerImpl.h"

using namespace Cangjie;
using namespace AST;

namespace {
bool IsLiteral(const Expr& expr)
{
    auto lit = DynamicCast<LitConstExpr>(&expr);
    if (!lit) {
        return false;
    }
    return !lit->siExpr;
}
} // namespace

bool TypeChecker::TypeCheckerImpl::ChkIfAvailableExpr(ASTContext& ctx, Ty& ty, IfAvailableExpr& ie)
{
    auto exprTy = SynIfAvailableExpr(ctx, ie);
    if (exprTy->IsInvalid()) {
        return false;
    }
    if (&ty != exprTy) {
        Sema::DiagMismatchedTypes(diag, ie, ty);
        return false;
    }
    return true;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynIfAvailableExpr(ASTContext& ctx, IfAvailableExpr& ie)
{
    auto arg = ie.GetArg();
    if (!arg) {
        return ie.ty = typeManager.GetInvalidTy();
    }
    bool res{true};
    if (arg->name.Empty()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_no_name, *ie.GetArg());
        res = false;
    }
    Synthesize(ctx, arg);
    ReplaceIdealTy(*arg->expr);
    ReplaceIdealTy(*arg);
    if (Ty::IsTyCorrect(arg->expr->ty) && !IsLiteral(*arg->expr)) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_not_literal, *ie.GetArg());
        res = false;
    }
    auto lambdaType = typeManager.GetFunctionTy({}, typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
    res = Check(ctx, lambdaType, ie.GetLambda1()) && res;
    res = Check(ctx, lambdaType, ie.GetLambda2()) && res;
    if (res) {
        return ie.ty = typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT);
    }
    return ie.ty = typeManager.GetInvalidTy();
}
