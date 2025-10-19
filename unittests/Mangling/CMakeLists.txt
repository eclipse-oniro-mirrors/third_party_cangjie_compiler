# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

add_executable(ManglingTests ManglingTests.cpp)
target_link_libraries(
    ManglingTests
    ${CMAKE_DL_LIBS}
    cangjie-lsp
    $<TARGET_OBJECTS:CangjieUnittestsMangle>
    GTest::gtest
    GTest::gtest_main)
add_test(NAME ManglingTests COMMAND ManglingTests)
