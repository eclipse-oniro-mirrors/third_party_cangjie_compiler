// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_AST2CHIR_CHECKER_H
#define CANGJIE_CHIR_AST2CHIR_CHECKER_H

#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/AST2CHIR/AST2CHIRNodeMap.h"
#include "cangjie/CHIR/Type/CHIRType.h"
#include "cangjie/CHIR/Value.h"

namespace Cangjie::CHIR {
bool AST2CHIRCheckCustomTypeDef(
    const AST::Node& astNode, const CustomTypeDef& chirNode, const AST2CHIRNodeMap<Value>& globalCache);
bool AST2CHIRCheckValue(const AST::Node& astNode, const Value& chirNode);
} // namespace Cangjie::CHIR

#endif