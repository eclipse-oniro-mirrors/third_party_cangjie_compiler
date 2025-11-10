// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Transformation/NoSideEffectMarker.h"

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/TaskQueue.h"
#include "cangjie/CHIR/Analysis/Utils.h"

namespace Cangjie::CHIR {

static const std::unordered_set<std::string> STD_NO_SIDE_EFFECT_LIST = {
#include "cangjie/CHIR/Transformation/StdNoSideEffectwhiteList.inc"
};

static const std::vector<std::string> NO_SIDE_EFFECT_PACKAGES = {"std"};

void NoSideEffectMarker::RunOnPackage(const Ptr<const Package>& package, bool isDebug)
{
    for (auto func : package->GetGlobalFuncs()) {
        RunOnFunc(func, isDebug);
    }
    for (auto imported : package->GetImportedVarAndFuncs()) {
        if (!imported->IsImportedVar()) {
            RunOnFunc(imported, isDebug);
        }
    }
}

void NoSideEffectMarker::RunOnFunc(const Ptr<Value>& value, bool isDebug)
{
    std::string packageName;
    std::string mangleName;
    if (auto func = DynamicCast<FuncBase>(value)) {
        packageName = func->GetPackageName();
        mangleName = func->GetRawMangledName();
    } else {
        return;
    }
    if (!CheckPackage(packageName)) {
        return;
    }
    if (STD_NO_SIDE_EFFECT_LIST.count(mangleName) == 0) {
        return;
    }
    value->EnableAttr(Attribute::NO_SIDE_EFFECT);

    if (isDebug) {
        std::string message = "[NoSideEffectMarker] The call to function " + value->GetSrcCodeIdentifier() +
            ToPosInfo(value->GetDebugLocation()) + " has been mark as no side effect.\n";
        std::cout << message;
    }
}

bool NoSideEffectMarker::CheckPackage(const std::string& packageName)
{
    for (const auto& whitePackage : NO_SIDE_EFFECT_PACKAGES) {
        if (whitePackage.size() > packageName.size()) {
            continue;
        }
        if (packageName.compare(0, whitePackage.length(), whitePackage) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace Cangjie::CHIR