// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_FUTURE_REMOVAL_H
#define CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_FUTURE_REMOVAL_H

#include "cangjie/CHIR/Expression/Terminator.h"
#include "cangjie/CHIR/Package.h"
#include "cangjie/CHIR/Value.h"

namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: replace future object with closure call in spawn expression.
 */
class RedundantFutureRemoval {
public:
    RedundantFutureRemoval(const Package& pkg, bool isDebug);

    /**
     * @brief Main process to do future remove in spawn expression.
     */
    void RunOnPackage();

private:
    void RunOnFunc(const Func& func);

    FuncBase* GetExecureClosureFunc() const;

    std::pair<LocalVar*, Apply*> CheckSpawnWithFuture(Expression& expr) const;

    void RewriteSpawnWithOutFuture(Spawn& spawnExpr, LocalVar& futureValue, Apply& apply);

    const Package& package;
    bool isDebug{false};

    FuncBase* executeClosure{nullptr};
};

}

#endif
