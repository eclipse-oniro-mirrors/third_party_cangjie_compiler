# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

set(FLATBUFFERS_SOURCE_DIR ${CMAKE_ARGV3})
set(FLATBUFFERS_PATCH ${CMAKE_ARGV4})

execute_process(COMMAND git diff --quiet
    WORKING_DIRECTORY ${FLATBUFFERS_SOURCE_DIR}
    RESULT_VARIABLE CJNATIVE_SOURCE_DIR_IS_MODIFIED)

if(CJNATIVE_SOURCE_DIR_IS_MODIFIED EQUAL 0)
    execute_process(
        COMMAND git apply --reject --whitespace=fix ${FLATBUFFERS_PATCH}
        WORKING_DIRECTORY ${FLATBUFFERS_SOURCE_DIR}
    )
endif()
