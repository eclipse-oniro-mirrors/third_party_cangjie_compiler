// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Annotation.h"
#include <iostream>
#include <sstream>

#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Value.h"

namespace Cangjie::CHIR {
std::string AnnotationMap::ToString() const
{
    std::stringstream ss;
    ss << loc.ToString();
    for (auto& pair : annotations) {
        if (ss.str() != "") {
            ss << ", ";
        }
        ss << pair.second->ToString();
    }
    return ss.str();
}

std::string SkipCheck::ToString()
{
    switch (kind) {
        case SkipKind::SKIP_DCE_WARNING:
            return "skip: dce warning";
        case SkipKind::SKIP_CODEGEN:
            return "skip: codegen";
        case SkipKind::SKIP_FORIN_EXIT:
            return "skip: vic (forin)";
        case SkipKind::SKIP_VIC:
            return "skip: vic";
        default:
            return "";
    }
}

std::string WrappedParentType::ToString()
{
    return "wrapped from: " + parentType->ToString();
}

std::string WrappedRawMethod::ToString()
{
    return "wrapped raw method: " + rawMethod->GetIdentifier();
}
} // namespace Cangjie::CHIR
