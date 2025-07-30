// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares some condition check helper macros.
 */

#ifndef CANGJIE_UTILS_CHECKUTILS_H
#define CANGJIE_UTILS_CHECKUTILS_H

#include <cassert>
#include <cstdlib>

#ifdef CMAKE_ENABLE_ASSERT
#define CJC_ASSERT(f)                                                                                                  \
    {                                                                                                                  \
        if (!(f)) {                                                                                                    \
            abort();                                                                                                   \
        }                                                                                                              \
    }
#define CJC_ABORT() abort()
#else
#ifdef NDEBUG
#define CJC_ASSERT(f) static_cast<void>(f)
#define CJC_ABORT()
#else
#define CJC_ASSERT(f) assert(f)
#define CJC_ABORT() abort()
#endif
#endif

#define CJC_NULLPTR_CHECK(p) CJC_ASSERT((p) != nullptr)

#endif
