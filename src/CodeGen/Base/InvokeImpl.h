// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements codegen for CHIR Invoke.
 */

#ifndef CANGJIE_INVOKE_H
#define CANGJIE_INVOKE_H

#include "llvm/IR/Value.h"

namespace Cangjie {
namespace CodeGen {
class IRBuilder2;
class CHIRInvokeWrapper;
class CHIRInvokeStaticWrapper;
llvm::Value* GenerateInvoke(IRBuilder2& irBuilder, const CHIRInvokeWrapper& invoke);
llvm::Value* GenerateInvokeStatic(IRBuilder2& irBuilder, const CHIRInvokeStaticWrapper& invokeStatic);
} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_INVOKE_H
