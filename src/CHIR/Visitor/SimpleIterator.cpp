// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements the SimpleIterator Class in CHIR.
 */

#include "cangjie/CHIR/Visitor/SimpleIterator.h"
#include "cangjie/CHIR/Expression/Terminator.h"
#include "cangjie/CHIR/Value.h"
#include "cangjie/CHIR/CHIRCasting.h"

using namespace Cangjie::CHIR;

std::vector<BlockGroup*> SimpleIterator::Iterate(const Expression& expr)
{
    auto kind{expr.GetExprKind()};
    if (kind == ExprKind::IF || kind == ExprKind::LOOP || Is<ForIn>(expr)) {
        return expr.GetBlockGroups();
    }
    return {};
}

std::vector<Block*> SimpleIterator::Iterate(const BlockGroup& blockGroup)
{
    return blockGroup.GetBlocks();
}

std::vector<Expression*> SimpleIterator::Iterate(const Block& block)
{
    return block.GetExpressions();
}
