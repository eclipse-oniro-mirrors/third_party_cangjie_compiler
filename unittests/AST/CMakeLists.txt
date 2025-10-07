# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

include_directories(${CMAKE_SOURCE_DIR}/src/Sema)
include_directories(${CMAKE_SOURCE_DIR}/src/AST)

add_executable(WalkerTests WalkerTest.cpp)
target_link_libraries(
    WalkerTests
    ${CMAKE_DL_LIBS}
    cangjie-lsp
    GTest::gtest
    GTest::gtest_main)
add_test(NAME WalkerTests COMMAND WalkerTests)

add_executable(CloneTests CloneTest.cpp)
target_link_libraries(
    CloneTests
    ${CMAKE_DL_LIBS}
    $<TARGET_OBJECTS:CangjieASTExtra>
    cangjie-lsp
    GTest::gtest
    GTest::gtest_main)
add_test(NAME CloneTests COMMAND CloneTests)
add_executable(CreateTests CreateTest.cpp)
target_link_libraries(
    CreateTests
    ${CMAKE_DL_LIBS}
    $<TARGET_OBJECTS:CangjieASTExtra>
    cangjie-lsp
    GTest::gtest
    GTest::gtest_main)
add_test(NAME CreateTests COMMAND CreateTests)

add_executable(SearchTest SearchTest.cpp)
target_link_libraries(
    SearchTest
    cangjie-lsp
    ${LINK_LIBS}
    boundscheck-static
    GTest::gtest
    GTest::gtest_main
    TestCompilerInstanceObject)
add_test(NAME SearchTest COMMAND SearchTest)

add_executable(SearchPlusTest SearchPlusTest.cpp)
target_link_libraries(
    SearchPlusTest
    cangjie-lsp
    ${LINK_LIBS}
    boundscheck-static
    GTest::gtest
    GTest::gtest_main
    TestCompilerInstanceObject)
add_test(NAME SearchPlusTest COMMAND SearchPlusTest)

add_executable(ASTToSourceTest ASTToSourceTest.cpp)
target_link_libraries(
    ASTToSourceTest
    ${CMAKE_DL_LIBS}
    cangjie-lsp
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ASTToSourceTest COMMAND ASTToSourceTest)
