// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/*
    type check
    block group block check
    block terminal check
*/

#ifndef CANGJIE_CHIR_IRCHECKER_H
#define CANGJIE_CHIR_IRCHECKER_H

#include <iostream>

#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/Option/Option.h"

namespace Cangjie::CHIR {
bool IRCheck(const class Package& root,
    const Cangjie::GlobalOptions& opts, CHIRBuilder& builder, std::ostream& out = std::cerr);
} // namespace Cangjie::CHIR

#endif