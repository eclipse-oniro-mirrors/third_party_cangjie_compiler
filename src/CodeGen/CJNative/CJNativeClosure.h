// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CJNATIVECLOSURE_H
#define CANGJIE_CJNATIVECLOSURE_H

#include "llvm/IR/Value.h"

#include "IRBuilder.h"
#include "cangjie/CHIR/Expression.h"

namespace Cangjie::CodeGen {
llvm::Value* GenerateClosure(IRBuilder2& irBuilder, const CHIR::Tuple& tuple);
} // namespace Cangjie::CodeGen

#endif // CANGJIE_CJNATIVECLOSURE_H
