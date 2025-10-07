// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares some common utility functions.
 */

#ifndef CANGJIE_BASIC_LINKAGE_H
#define CANGJIE_BASIC_LINKAGE_H

namespace Cangjie {
/**
 * Represent the linkage of the Decl.
 */
enum class Linkage {
    WEAK_ODR,      /**< weak_odr linkage. */
    EXTERNAL,      /**< External linkage. */
    INTERNAL,      /**< Internal linkage. */
    LINKONCE_ODR,  /**< linkonce_odr linkage. */
    EXTERNAL_WEAK, /**< external_weak linkage */
};
} // namespace Cangjie

#endif // CANGJIE_BASIC_LINKAGE_H
