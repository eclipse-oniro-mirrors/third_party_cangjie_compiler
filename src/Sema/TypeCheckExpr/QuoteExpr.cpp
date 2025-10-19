// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "TypeCheckerImpl.h"

#include "Diags.h"

using namespace Cangjie;
using namespace Sema;

namespace {
void ChkIfImportLibAST(DiagnosticEngine& diag, const ImportManager& im, const QuoteExpr& qe)
{
    if (qe.GetFullPackageName() == AST_PACKAGE_NAME) {
        return;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == AST_PACKAGE_NAME) {
            return;
        }
    }
    diag.DiagnoseRefactor(DiagKindRefactor::sema_use_expr_without_import, qe, "std.ast", "quote");
}
} // namespace

bool TypeChecker::TypeCheckerImpl::ChkQuoteExpr(ASTContext& ctx, Ty& target, QuoteExpr& qe)
{
    ChkIfImportLibAST(diag, importManager, qe);
    if (!Ty::IsTyCorrect(Synthesize(ctx, &qe))) {
        return false;
    }
    if (!typeManager.IsSubtype(qe.ty, &target)) {
        DiagMismatchedTypes(diag, qe, target);
        qe.ty = TypeManager::GetInvalidTy();
        return false;
    }
    if (qe.desugarExpr) {
        if (!Check(ctx, &target, qe.desugarExpr.get())) {
            qe.ty = TypeManager::GetInvalidTy();
            return false;
        }
    }
    return true;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynQuoteExpr(ASTContext& ctx, QuoteExpr& qe)
{
    ChkIfImportLibAST(diag, importManager, qe);
    if (qe.desugarExpr) {
        qe.ty = Synthesize(ctx, qe.desugarExpr.get());
    } else {
        qe.ty = TypeManager::GetInvalidTy();
    }
    return qe.ty;
}
