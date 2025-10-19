// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_TRANSFORMATION_MARK_CLASS_HASINITED_H
#define CANGJIE_CHIR_TRANSFORMATION_MARK_CLASS_HASINITED_H

#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/CHIR/Package.h"

namespace Cangjie::CHIR {
/**
 * CHIR normal Pass: add has invited flag to class which has finalizer, in case of finalize before init.
 */
class MarkClassHasInited {
public:
    /**
     * @brief Main process to add has invited flag to class.
     * @param package package to do optimization.
     * @param builder CHIR builder for generating IR.
     */
    static void RunOnPackage(const Package& package, CHIRBuilder& builder);
};
} // namespace Cangjie::CHIR

#endif
