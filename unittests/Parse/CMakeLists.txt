# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

add_executable(ParserTests ParserTest.cpp)
target_link_libraries(
    ParserTests
    cangjie-lsp
    ${CMAKE_DL_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ParserTests COMMAND ParserTests)

add_executable(ParseTypeTests ParseTypeTest.cpp)
target_link_libraries(
    ParseTypeTests
    cangjie-lsp
    ${CMAKE_DL_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ParseTypeTests COMMAND ParseTypeTests)

add_executable(ParseTypeAliasTest ParseTypeAliasTest.cpp)
target_link_libraries(
    ParseTypeAliasTest
    cangjie-lsp
    ${CMAKE_DL_LIBS}
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ParseTypeAliasTest COMMAND ParseTypeAliasTest)

add_executable(ParseCommentTest ParseCommentTest.cpp)
target_link_libraries(
    ParseCommentTest
    cangjie-lsp
    ${LINK_LIBS}
    boundscheck-static
    GTest::gtest
    GTest::gtest_main
    TestCompilerInstanceObject)

add_test(NAME ParseCommentTest COMMAND ParseCommentTest)