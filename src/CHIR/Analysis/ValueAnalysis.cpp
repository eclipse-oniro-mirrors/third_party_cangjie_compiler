// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Analysis/ValueAnalysis.h"

using namespace Cangjie::CHIR;

template <typename ValueDomain>
typename State<ValueDomain>::ChildrenMap ValueAnalysis<ValueDomain>::globalChildrenMap{};
template <typename ValueDomain>
typename State<ValueDomain>::AllocatedRefMap ValueAnalysis<ValueDomain>::globalAllocatedRefMap{};
template <typename ValueDomain>
typename State<ValueDomain>::AllocatedObjMap ValueAnalysis<ValueDomain>::globalAllocatedObjMap{};
template <typename ValueDomain>
std::vector<std::unique_ptr<Ref>> ValueAnalysis<ValueDomain>::globalRefPool{};
template <typename ValueDomain>
std::vector<std::unique_ptr<AbstractObject>> ValueAnalysis<ValueDomain>::globalAbsObjPool{};
template <typename ValueDomain>
State<ValueDomain> ValueAnalysis<ValueDomain>::globalState{
    &globalChildrenMap, &globalAllocatedRefMap, &globalAllocatedObjMap, &globalRefPool, &globalAbsObjPool};