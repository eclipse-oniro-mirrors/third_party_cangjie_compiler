// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::Visit(const AST::PointerExpr& expr)
{
    auto ty = TranslateType(*expr.ty);
    CHIR::IntrinsicKind intrinsicKind;
    std::vector<Value*> args{};

    if (expr.arg) {
        intrinsicKind = IntrinsicKind::CPOINTER_INIT1;
        auto loc = TranslateLocation(*expr.arg);
        Value* argVal = nullptr;
        if (expr.arg->withInout) {
            auto argLeftValInfo = TranslateExprAsLeftValue(*expr.arg->expr);
            argVal = argLeftValInfo.base;
            // polish this
            if (!argLeftValInfo.path.empty()) {
                auto lhsCustomType = StaticCast<CustomType*>(argVal->GetType()->StripAllRefs());
                if (argVal->GetType()->IsRef()) {
                    argVal = CreateGetElementRefWithPath(TranslateLocation(expr), argVal,
                        argLeftValInfo.path, currentBlock, *lhsCustomType);
                } else {
                    auto memberType = lhsCustomType->GetInstMemberTypeByPath(argLeftValInfo.path, builder);
                    auto getMember = CreateAndAppendExpression<Field>(
                        TranslateLocation(expr), memberType, argVal, argLeftValInfo.path, currentBlock);
                    argVal = getMember->GetResult();
                }
            }
            auto ty1 = TranslateType(*expr.arg->ty);
            argVal = CreateAndAppendExpression<Intrinsic>(
                loc, ty1, CHIR::IntrinsicKind::INOUT_PARAM, std::vector<Value*>{argVal}, currentBlock)
                         ->GetResult();
        } else {
            argVal = TranslateASTNode(*expr.arg, *this);
        }
        CJC_NULLPTR_CHECK(argVal);
        argVal = GenerateLoadIfNeccessary(*argVal, false, false, expr.arg->withInout, loc);
        args.emplace_back(argVal);
    } else {
        intrinsicKind = IntrinsicKind::CPOINTER_INIT0;
    }
    const auto& loc = TranslateLocation(expr);
    return CreateAndAppendExpression<Intrinsic>(loc, ty, intrinsicKind, args, currentBlock)->GetResult();
}