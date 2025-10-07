// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::Visit(const AST::UnaryExpr& unaryExpr)
{
    auto chirType = TranslateType(*unaryExpr.ty);
    ExprKind kd = ExprKind::INVALID;
    if (unaryExpr.op == Cangjie::TokenKind::NOT) {
        kd = unaryExpr.ty->IsBoolean() ? ExprKind::NOT : ExprKind::BITNOT;
    } else if (unaryExpr.op == Cangjie::TokenKind::SUB) {
        kd = ExprKind::NEG;
    } else {
        CJC_ASSERT(false && "Visit UnaryExpr: invalid unary operation!");
    }
    auto chirExpr = TranslateExprArg(*unaryExpr.expr);
    const auto& loc = TranslateLocation(unaryExpr.begin, unaryExpr.end);

    auto ofs = unaryExpr.overflowStrategy;
    bool mayHaveException = OverloadableExprMayThrowException(unaryExpr, *chirType);
    auto opLoc = TranslateLocation(*unaryExpr.expr);
    const auto& operatorLoc = GetOperatorLoc(unaryExpr);
    return TryCreateWithOV<UnaryExpression>(
        currentBlock, mayHaveException, ofs, operatorLoc, loc, chirType, kd, chirExpr)
        ->GetResult();
}