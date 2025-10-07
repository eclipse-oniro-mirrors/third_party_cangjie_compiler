// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file tuple domain.
 */

#include "cangjie/CHIR/Analysis/TupleDomain.h"

namespace Cangjie::CHIR {
TupleDomain::TupleDomain(size_t fieldNum) : num{fieldNum}, val{fieldNum}
{
}

TupleDomain::TupleDomain(size_t fieldNum, std::vector<FieldType> fieldValue) : num(fieldNum), val(std::move(fieldValue))
{
}

TupleDomain::TupleDomain(const Tuple& tuple)
    : num(tuple.GetNumOfOperands()), val{tuple.GetOperands().begin(), tuple.GetOperands().end()}
{
}

TupleDomain& TupleDomain::operator=(TupleDomain&& other)
{
    CJC_ASSERT(num == other.num);
    val = std::move(other.val);
    return *this;
}

bool TupleDomain::IsTop() const
{
    return false;
}

size_t TupleDomain::FieldNum() const
{
    return num;
}

TupleDomain::FieldType TupleDomain::operator[](size_t index) const
{
    return val[index];
}

TupleDomain::FieldType& TupleDomain::operator[](size_t index)
{
    return val[index];
}

std::vector<TupleDomain::FieldType>::iterator TupleDomain::begin()
{
    return val.begin();
}

std::vector<TupleDomain::FieldType>::iterator TupleDomain::end()
{
    return val.end();
}
}  // namespace Cangjie::CHIR