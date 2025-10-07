# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

file(GLOB SEMA_SRC *.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(Desugar)
add_subdirectory(FFI)
add_subdirectory(GenericInstantiation)
add_subdirectory(InheritanceChecker)
add_subdirectory(LegalityOfUsage)
add_subdirectory(Test)
add_subdirectory(TypeCheckExpr)

add_library(CangjieSema OBJECT ${SEMA_SRC})
target_compile_options(CangjieSema PRIVATE ${CJC_EXTRA_WARNINGS})
