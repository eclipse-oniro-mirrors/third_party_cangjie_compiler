// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Type/Type.h"

using namespace Cangjie::CHIR;

bool CHIRTypeCompare::operator()(const Type* key1, const Type* key2) const
{
    return key1->ToString() < key2->ToString();
}
