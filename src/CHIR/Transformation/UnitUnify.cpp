// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/UnitUnify.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Visitor/Visitor.h"

namespace Cangjie::CHIR {
UnitUnify::UnitUnify(CHIRBuilder& builder) : builder(builder)
{
}

void UnitUnify::RunOnPackage(const Ptr<const Package>& package, bool isDebug)
{
    for (auto func : package->GetGlobalFuncs()) {
        RunOnFunc(func, isDebug);
    }
}

void UnitUnify::RunOnFunc(const Ptr<Func>& func, bool isDebug)
{
    Ptr<Constant> optUnit;
    auto preAcation = [this, isDebug, &optUnit](Expression& expr) {
        if (Is<GetRTTI>(expr) || Is<GetRTTIStatic>(expr)) {
            return VisitResult::CONTINUE;
        }
        if (expr.GetResult() != nullptr && expr.GetResult()->GetType()->GetTypeKind() == Type::TypeKind::TYPE_UNIT &&
            !expr.GetResult()->GetUsers().empty()) {
            LoadOrCreateUnit(optUnit, expr.GetParentBlockGroup());
            expr.GetResult()->ReplaceWith(*optUnit->GetResult(), expr.GetParentBlockGroup());
            if (isDebug) {
                std::cout << "[UnitUnify] unit unify" << ToPosInfo(expr.GetDebugLocation()) << ".\n";
            }
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(*func, preAcation);
}

void UnitUnify::LoadOrCreateUnit(Ptr<Constant>& constant, const Ptr<BlockGroup>& group)
{
    if (constant != nullptr) {
        return;
    }
    auto entryBlock = group->GetEntryBlock();
    constant = builder.CreateConstantExpression<UnitLiteral>(builder.GetUnitTy(), entryBlock);
    constant->MoveBefore(entryBlock->GetExpressions()[0]);
}
} // namespace Cangjie::CHIR