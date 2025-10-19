// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "Desugar/AfterTypeCheck.h"
#include "cangjie/AST/Create.h"

#include "TypeCheckUtil.h"

using namespace Cangjie;
using namespace TypeCheckUtil;

namespace Cangjie::Sema::Desugar::AfterTypeCheck {
OwnedPtr<TypePattern> CreateRuntimePreparedTypePattern(
    TypeManager& typeManager, OwnedPtr<Pattern> pattern, OwnedPtr<Type>  type, Expr& selector)
{
    auto typePattern = CreateTypePattern(std::move(pattern), std::move(type), selector);
    typePattern->matchBeforeRuntime = typeManager.IsSubtype(selector.ty, typePattern->ty, true, false);
    typePattern->needRuntimeTypeCheck =
        !typePattern->matchBeforeRuntime && IsNeedRuntimeCheck(typeManager, *selector.ty, *typePattern->ty);
    return typePattern;
}
} // namespace Cangjie::Sema::Desugar::AfterTypeCheck
