// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/Driver/DriverOptions.h"
#include "gtest/gtest.h"

#include <memory>

using namespace Cangjie;

class OptionTest : public ::testing::Test {
protected:
    void SetUp() override { options = std::make_unique<DriverOptions>(); }

    std::unique_ptr<DriverOptions> options;
};

TEST_F(OptionTest, OptionInit)
{
    option->Init(OptionCode::OC_BACKEND_ARGS, "-O3", "this is description");

    EXPECT_EQ(option->desc, "this is description");
    EXPECT_EQ(option->value, "-O3");
    EXPECT_EQ(option->code, OptionCode::OC_BACKEND_ARGS);
}

TEST_F(OptionTest, DriverOptionsParse)
{
    std::vector<std::string> args = {"./cjc"};

    std::vector<std::string> files = {"main.cj", "io.cj"};

    /// Correct Parsed
    bool ret = options->Parse(args, files);

    EXPECT_TRUE(ret);
    EXPECT_FALSE(options->ifEmitIR);
    EXPECT_TRUE(options->ifEmitBin);
    EXPECT_FALSE(options->ifDumpAST);
    EXPECT_FALSE(options->ifDumpIR);

    EXPECT_EQ(options->inputFiles[0], "main.cj");
    EXPECT_EQ(options->inputFiles[1], "io.cj");
    EXPECT_EQ(options->inputDir, ".");

    args.emplace_back("--dump-ir");
    ret = options->Parse(args, files);
    EXPECT_TRUE(options->ifDumpIR);

    args.emplace_back("--dump-ast");
    ret = options->Parse(args, files);
    EXPECT_TRUE(options->ifDumpAST);

#ifdef _WIN32
    args.emplace_back("-output=.\\test\\test.wasm");
    ret = options->Parse(args, files);
    EXPECT_EQ(options->outputDir, ".\\test");
#else
    args.emplace_back("-output=./test/test.wasm");
    ret = options->Parse(args, files);
    EXPECT_EQ(options->outputDir, "./test");
#endif

    EXPECT_EQ(options->outputName, "test");

    /// Invalid parameter
    args.emplace_back("-iamnotvalid");
    ret = options->Parse(args, files);

    EXPECT_FALSE(ret);
}

TEST_F(OptionTest, AdjustOption)
{
    vector<string> args = {"./cjc", "--dump-ir"};

    vector<string> files = {"main.cj", "io.cj"};

    bool ret = options->Parse(args, files);
    options->Adjust();
    EXPECT_FALSE(options->ifEmitBin);

#ifdef _WIN32
    args.emplace_back("-output=.\\test\\test.wasm");
    ret = options->Parse(args, files);
    options->Adjust();
    EXPECT_EQ(options->outputDir, ".\\test");
#else
    args.emplace_back("-output=./test/test.wasm");
    ret = options->Parse(args, files);
    options->Adjust();
    EXPECT_EQ(options->outputDir, "./test");
#endif
    EXPECT_TRUE(ret);
    EXPECT_EQ(options->outputName, "test");
}
