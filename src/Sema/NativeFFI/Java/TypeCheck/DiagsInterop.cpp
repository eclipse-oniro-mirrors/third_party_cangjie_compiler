// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "DiagsInterop.h"

namespace Cangjie::Interop::Java {

void DiagJavaHasDefaultNoArgs(DiagnosticEngine& diag, const Node& node)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_java_has_default_annotation_args, node);
}

void DiagJavaHasDefaultIncorrectScope(DiagnosticEngine& diag, const Node& node)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_java_has_default_annotation_is_in_wrong_place, node);
}

void DiagJavaHasDefaultConflictWithStatic(DiagnosticEngine& diag, const Node& node)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_java_has_default_conflict_with_static, node);
}

} // namespace Cangjie::Interop::Java
