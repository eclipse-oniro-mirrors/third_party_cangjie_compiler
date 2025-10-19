# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

add_executable(DriverTests DriverTest.cpp ToolchainTest.cpp TempFileManagerTest.cpp ${CANGJIE_SRC_OBJECTS})
target_link_libraries(
    DriverTests
    GTest::gtest
    GTest::gtest_main
    ${LINK_LIBS})
add_test(NAME DriverTests COMMAND DriverTests)
