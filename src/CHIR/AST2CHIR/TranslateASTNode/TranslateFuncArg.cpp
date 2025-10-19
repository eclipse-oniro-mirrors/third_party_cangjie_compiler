// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"

#include "cangjie/CHIR/IntrinsicKind.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::Visit(const AST::FuncArg& arg)
{
    auto val = TranslateExprArg(*arg.expr);
    // not handled here; see Translator::TranslateTrivialArgWithNoSugar
    CJC_ASSERT(!arg.withInout);
    return val;
}
