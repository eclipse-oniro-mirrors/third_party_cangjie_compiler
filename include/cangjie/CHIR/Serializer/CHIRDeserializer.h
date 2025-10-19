// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_SERIALIZER_DESERIALIZER_H
#define CANGJIE_CHIR_SERIALIZER_DESERIALIZER_H

#include "cangjie/CHIR/CHIRBuilder.h"

#include <string>

namespace Cangjie::CHIR {
class CHIRDeserializer {
    class CHIRDeserializerImpl;

public:
    static bool Deserialize(const std::string& fileName, Cangjie::CHIR::CHIRBuilder& chirBuilder, ToCHIR::Phase& phase,
        bool compilePlatform = false);

private:
    explicit CHIRDeserializer()
    {
    }
};

} // namespace Cangjie::CHIR

#endif
