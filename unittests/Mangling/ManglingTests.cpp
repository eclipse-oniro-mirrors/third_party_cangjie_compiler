// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include <cstdio>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

#define private public
#include "cangjie/Mangle/BaseMangler.h"

using namespace Cangjie;

TEST(UtilsTest, GetGenericTyConstraintStr)
{
    std::string ret = MangleUtils::GetGenericTyConstraintStr("", "");
    EXPECT_EQ("", ret);

    ret = MangleUtils::GetGenericTyConstraintStr("T", "<E> where E <: bbb");
    EXPECT_EQ("", ret);

    ret = MangleUtils::GetGenericTyConstraintStr("T", "<T> where T");
    EXPECT_EQ("", ret);

    ret = MangleUtils::GetGenericTyConstraintStr("T", "<T, E> where T <: aaa, E <: bbb");
    EXPECT_EQ("aaa", ret);
}
