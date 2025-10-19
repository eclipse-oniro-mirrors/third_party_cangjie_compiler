// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/Basic/Print.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Cangjie {

#ifdef _WIN32

// Enables OS earlier than Windos10 1511 to compile normally
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

const int WINDOWS_10_VERSION_1511_BUILD_NUMBER = 10586;
const int WINDOWS_10 = 10;
ColorSingleton::ColorSingleton()
{
    DWORD stdoutMode;
    DWORD stderrMode;
    GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &stdoutMode);
    GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), &stderrMode);

    // store initial console mode
    initialStdoutMode = stdoutMode;
    initialStderrMode = stderrMode;

    // get current Windows os version
    auto osVersion = Utils::GetOSVersion();
    if (osVersion.dwMajorVersion >= WINDOWS_10 && osVersion.dwBuildNumber >= WINDOWS_10_VERSION_1511_BUILD_NUMBER) {
        // current Windows os version is or newer than Windows10 (version 1511)
        stdoutMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        stderrMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), stdoutMode);
        SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), stderrMode);

        ANSI_COLOR_RESET = "\x1b[0m";
        ANSI_COLOR_BRIGHT = "\x1b[1m";
        ANSI_COLOR_BLACK = "\x1b[30m";
        ANSI_COLOR_RED = "\x1b[31m";
        ANSI_COLOR_GREEN = "\x1b[32m";
        ANSI_COLOR_YELLOW = "\x1b[33m";
        ANSI_COLOR_BLUE = "\x1b[34m";
        ANSI_COLOR_MAGENTA = "\x1b[35m";
        ANSI_COLOR_CYAN = "\x1b[36m";
        ANSI_COLOR_WHITE = "\x1b[37m";

        ANSI_COLOR_WHITE_BACKGROUND_BLACK_FOREGROUND = "\x1b[30;47m";
    }
};

ColorSingleton::~ColorSingleton()
{
    // restore to the initial console mode
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), initialStdoutMode);
    SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), initialStderrMode);
}
#else
ColorSingleton::ColorSingleton()
{
    ANSI_COLOR_RESET = "\x1b[0m";
    ANSI_COLOR_BRIGHT = "\x1b[1m";
    ANSI_COLOR_BLACK = "\x1b[30m";
    ANSI_COLOR_RED = "\x1b[31m";
    ANSI_COLOR_GREEN = "\x1b[32m";
    ANSI_COLOR_YELLOW = "\x1b[33m";
    ANSI_COLOR_BLUE = "\x1b[34m";
    ANSI_COLOR_MAGENTA = "\x1b[35m";
    ANSI_COLOR_CYAN = "\x1b[36m";
    ANSI_COLOR_WHITE = "\x1b[37m";

    ANSI_COLOR_WHITE_BACKGROUND_BLACK_FOREGROUND = "\x1b[30;47m";
};

ColorSingleton::~ColorSingleton(){};
#endif
} // namespace Cangjie
