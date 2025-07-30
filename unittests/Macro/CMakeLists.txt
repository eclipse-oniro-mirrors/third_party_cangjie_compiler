# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

include_directories(${CMAKE_SOURCE_DIR}/src/Sema)
if(CANGJIE_CODEGEN_CJNATIVE_BACKEND)
    add_executable(TokenSerializationTests TokenSerializationTest.cpp)
    add_executable(NodeSerializationTests NodeSerializationTest.cpp)
    add_executable(MacroTests MacroTest.cpp)

    target_link_libraries(
        TokenSerializationTests
        cangjie-lsp
        ${LINK_LIBS}
        boundscheck-static
        GTest::gtest
        GTest::gtest_main)
    target_link_libraries(
        NodeSerializationTests
        cangjie-lsp
        ${LINK_LIBS}
        boundscheck-static
        GTest::gtest
        GTest::gtest_main)
    target_link_libraries(
        MacroTests
        cangjie-lsp
        ${LINK_LIBS}
        boundscheck-static
        GTest::gtest
        GTest::gtest_main
        TestCompilerInstanceObject)

    add_dependencies(NodeSerializationTests CangjieFlatbuffersHeaders)
    target_include_directories(NodeSerializationTests PRIVATE ${FLATBUFFERS_INCLUDE_DIR})

    add_test(NAME TokenSerializationTests COMMAND TokenSerializationTests)
    add_test(NAME NodeSerializationTests COMMAND NodeSerializationTests)
    add_test(NAME MacroTests COMMAND MacroTests)

endif()
