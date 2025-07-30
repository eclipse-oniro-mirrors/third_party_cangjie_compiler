# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

add_executable(UnicodeTests UnicodeTests.cpp)
target_link_libraries(
    UnicodeTests
    ${CMAKE_DL_LIBS}
    $<TARGET_OBJECTS:CangjieUnicodeUtils>
    GTest::gtest
    GTest::gtest_main)
add_test(NAME UnicodeTests COMMAND UnicodeTests)

add_executable(GeneratedNFCTests NFC.cpp)
target_link_libraries(
    GeneratedNFCTests
    ${CMAKE_DL_LIBS}
    $<TARGET_OBJECTS:CangjieUnicodeUtils>
    GTest::gtest
    GTest::gtest_main)
target_include_directories(GeneratedNFCTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/Utils/UnicodeTables)
add_test(NAME GeneratedNFCTests COMMAND GeneratedNFCTests)
# regenerate unicode test data tables
if(CANGJIE_GENERATE_UNICODE_TABLE)
    add_dependencies(GeneratedNFCTests GenerateNFCTestData)
endif()

add_executable(UnicodeWidthTests UnicodeWidthTests.cpp)
target_link_libraries(
    UnicodeWidthTests
    ${CMAKE_DL_LIBS}
    $<TARGET_OBJECTS:CangjieUnicodeUtils>
    GTest::gtest
    GTest::gtest_main)
add_test(NAME UnicodeWidthTests COMMAND UnicodeWidthTests)
if(CANGJIE_GENERATE_UNICODE_TABLE)
    add_dependencies(UnicodeWidthTests GenerateWidthData)
endif()

set(BASE_UTIL_SRC)
