// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "TypeCheckerImpl.h"

#include "Diags.h"
#include "TypeCheckUtil.h"

#include "cangjie/AST/RecoverDesugar.h"

using namespace Cangjie;
using namespace Sema;
using namespace TypeCheckUtil;

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynTypeConvExpr(ASTContext& ctx, TypeConvExpr& tce)
{
    CJC_NULLPTR_CHECK(tce.expr);
    CJC_NULLPTR_CHECK(tce.type);
    Synthesize(ctx, tce.expr.get());
    ReplaceIdealTy(*tce.expr);
    if (tce.type->astKind == ASTKind::PRIMITIVE_TYPE) {
        return SynNumTypeConvExpr(tce);
    }

    // The TypeConvExpr supports conversion between primitive types and conversion from CPointer to CFunc.
    // Therefore, the function should be returned in either of the above two branches.
    // Otherwise, there must be errors reported by other modules or logic codes.
    tce.ty = TypeManager::GetInvalidTy();
    return tce.ty;
}

bool TypeChecker::TypeCheckerImpl::ChkCFuncConstructorExpr(ASTContext& ctx, CallExpr& ce)
{
    auto callee = DynamicCast<RefExpr>(&*ce.baseFunc);
    if (!callee) {
        ce.ty = TypeManager::GetInvalidTy();
        return false;
    }
    ce.ty = Synthesize(ctx, ce.baseFunc.get());
    if (!Ty::IsTyCorrect(ce.baseFunc->ty) || !Ty::IsTyCorrect(ce.ty)) {
        ce.ty = TypeManager::GetInvalidTy();
        return false;
    }
    if (ce.ty->IsCFunc()) {
        if (ce.args.size() != 1) {
            diag.Diagnose(*ce.baseFunc, DiagKind::sema_cfunc_too_many_arguments);
            ce.ty = TypeManager::GetInvalidTy();
            return false;
        }
        Synthesize(ctx, ce.args[0]);
        if (!Ty::IsTyCorrect(ce.args[0]->ty)) {
            ce.ty = TypeManager::GetInvalidTy();
            return false;
        }
        bool res{true};
        if (!ce.args[0]->name.Empty()) {
            diag.Diagnose(ce.args[0]->name.Begin(), ce.args[0]->name.End(), DiagKind::sema_unknown_named_argument,
                ce.args[0]->name.Val());
            // the program is invalid, yet the type check can still pass if all other checks hold, do not goto INVALID
            // here
            res = false;
        }
        // only CFunc<...>(CPointer(...)) is valid
        if (ce.args[0]->ty->IsPointer()) {
            ce.ty = ce.baseFunc->ty;
            return res;
        }
    }
    // Otherwise, report error and return a invalid ty.
    diag.Diagnose(*ce.args[0], DiagKind::sema_cfunc_ctor_must_be_cpointer);
    ce.ty = TypeManager::GetInvalidTy();
    return false;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynNumTypeConvExpr(TypeConvExpr& tce)
{
    tce.ty = TypeManager::GetPrimitiveTy(StaticCast<PrimitiveType*>(tce.type.get())->kind);
    if (!Ty::IsTyCorrect(tce.expr->ty) || !Ty::IsTyCorrect(tce.ty)) {
        tce.ty = TypeManager::GetInvalidTy();
        return tce.ty;
    }
    // Case 0: expr is of Nothing type, e.g., `UInt32(return)`
    bool isExprNothing = (tce.ty->kind == TypeKind::TYPE_RUNE || tce.ty->IsNumeric()) && tce.expr->ty->IsNothing();
    // Case 1: Rune to UInt32, e.g., `UInt32('a')`
    bool isRuneToUInt32 = tce.ty->kind == TypeKind::TYPE_UINT32 && tce.expr->ty->kind == TypeKind::TYPE_RUNE;
    // Case 2: Integer to Rune, e.g., `Rune(97)`
    bool isIntegerToChar = tce.ty->kind == TypeKind::TYPE_RUNE && tce.expr->ty->IsInteger();
    // Case 3: convert between numeric types
    bool isBetweenNumeric = tce.ty->IsNumeric() && tce.expr->ty->IsNumeric();
    if (isExprNothing || isRuneToUInt32 || isIntegerToChar || isBetweenNumeric) {
        return tce.ty;
    }
    // Otherwise, return false.
    if (!CanSkipDiag(*tce.expr)) {
        diag.Diagnose(*tce.expr, DiagKind::sema_numeric_convert_must_be_numeric);
    }
    tce.ty = TypeManager::GetInvalidTy();
    return tce.ty;
}

bool TypeChecker::TypeCheckerImpl::ChkTypeConvExpr(ASTContext& ctx, Ty& targetTy, TypeConvExpr& tce)
{
    // Additionally, given a context type T0 and an expression T1(t), since T1(t) : T1, we always require T1 <: T0.
    if (Ty::IsTyCorrect(SynTypeConvExpr(ctx, tce)) && typeManager.IsSubtype(tce.ty, &targetTy)) {
        return true;
    } else {
        if (!CanSkipDiag(tce)) {
            DiagMismatchedTypes(diag, tce, targetTy);
        }
        tce.ty = TypeManager::GetInvalidTy();
        return false;
    }
}
