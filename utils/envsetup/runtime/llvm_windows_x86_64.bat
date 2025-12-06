@REM Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
@REM This source file is part of the Cangjie project, licensed under Apache-2.0
@REM with Runtime Library Exception.
@REM
@REM See https://cangjie-lang.cn/pages/LICENSE for license information.

@REM The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

@REM This script needs to be placed in the root directory of installation of Cangjie compiler and libraries.

@echo off
REM Windows searches for both binaries and libs in %Path%
set "PATH=%~dp0lib\windows_x86_64_cjnative;%PATH%"