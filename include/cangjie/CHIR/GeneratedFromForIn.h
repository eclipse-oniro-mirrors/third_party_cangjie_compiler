// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_GENERATEDFROMFORIN_H
#define CANGJIE_CHIR_GENERATEDFROMFORIN_H
#include "cangjie/CHIR/Annotation.h"

namespace Cangjie::CHIR {
/// This type is used to track CHIR node generated from for-in expr translation, so const propagation may optimise on
/// them even in O0.
struct GeneratedFromForIn final : public Annotation {
    GeneratedFromForIn() : value{false} {}
    explicit GeneratedFromForIn(bool) : value{true} {}
    std::unique_ptr<Annotation> Clone() override
    {
        return std::make_unique<GeneratedFromForIn>();
    }
 
    std::string ToString() override
    {
        return "// generated-from-forin ";
    }
 
    static bool Extract(const GeneratedFromForIn* label)
    {
        return label->value;
    }
 
private:
    bool value;
};
}
#endif
