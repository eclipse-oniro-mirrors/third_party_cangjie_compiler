// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/Driver/Driver.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <string>
#include <vector>

#include "cangjie/Driver/Utils.h"

using namespace Cangjie;

class DriverTest : public ::testing::Test {
public:
    std::unique_ptr<Driver> driver;
};

TEST_F(DriverTest, ExecuateCompilation) {}

TEST_F(DriverTest, GetSingleQuotedTest)
{
    const std::string singleQuote{"\\'"};
    EXPECT_EQ(GetSingleQuoted("abcde"), "'abcde'");
    EXPECT_EQ(GetSingleQuoted("'; ls"), "''" + singleQuote + "'; ls'");
    EXPECT_EQ(GetSingleQuoted("'wrapped'"), std::string{"''" + singleQuote + "'wrapped'" + singleQuote + "''"});
    EXPECT_EQ(GetSingleQuoted("start'wrapped'end"),
        std::string{"'start'" + singleQuote + "'wrapped'" + singleQuote + "'end'"});
    EXPECT_EQ(GetSingleQuoted("end'"), std::string{"'end'" + singleQuote + "''"});
}