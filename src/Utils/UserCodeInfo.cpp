// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "UserCodeInfo.h"

namespace Cangjie {

void UserCodeInfo::RecordInfo(const std::string& item, int64_t value)
{
    codeInfo.emplace_back(item, value);
}

#ifdef CANGJIE_WRITE_PROFILE
std::string UserCodeInfo::GetJson() const
{
    std::string output;
    output += "{";
    for (auto& it : codeInfo) {
        output += ("\n   \"" + it.first + "\": " + std::to_string(it.second) + ",");
    }
    if (!codeInfo.empty()) {
        output.pop_back();
    }
    output += "\n}\n";
    return output;
}
#endif

std::string UserCodeInfo::GetFlat() const
{
    std::string output;
    if (packageName.empty()) {
        output += "================ Code Info ================\n";
    } else {
        output += "================ Code Info of [ " + packageName + " ] ================\n";
    }
    for (const auto& it : codeInfo) {
#ifdef _WIN32
        output += ("[ " + it.first + " ] " + std::to_string(it.second) + "\n");
#else
        output += ("[ \033[32;1m" + it.first + "\033[0m ] " + std::to_string(it.second) + "\n");
#endif
    }
    return output;
}
} // namespace Cangjie
