// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares JavaFFI annotations checks
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_TYPE_CHECK_ANNOTATION_H
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_TYPE_CHECK_ANNOTATION_H

#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"

namespace Cangjie::Interop::Java {

void CheckJavaHasDefaultAnnotation(Cangjie::DiagnosticEngine& diag, const Cangjie::AST::Annotation& anno,
    const Cangjie::AST::Decl& decl);

} // namespace Cangjie::Interop::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_TYPE_CHECK_ANNOTATION_H
