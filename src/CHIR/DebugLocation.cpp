// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements the DebugLocation in CHIR.
 */

#include "cangjie/CHIR/DebugLocation.h"
#include <sstream>

using namespace Cangjie::CHIR;

std::string DebugLocation::ToString() const
{
#ifdef _WIN32
    const std::string dirSeparator = "\\/";
#else
    const std::string dirSeparator = "/";
#endif
    std::stringstream ss;
    std::string name = absPath->substr(absPath->find_last_of(dirSeparator) + 1);
    ss << "loc: \"" << name << "\"-" << beginPos.line << "-" << beginPos.column;
    if (!scopeInfo.empty()) {
        ss << ", scope: " << scopeInfo[0];
    }
    for (size_t t = 1; t < scopeInfo.size(); t++) {
        ss << "-" << scopeInfo[t];
    }
    return ss.str();
}
