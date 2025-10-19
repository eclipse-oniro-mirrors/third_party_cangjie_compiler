// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_SERIALIZER_CHIRSERIALIZER_H
#define CANGJIE_CHIR_SERIALIZER_CHIRSERIALIZER_H

#include "cangjie/CHIR/Package.h"

#include <string>

namespace Cangjie::CHIR {

class CHIRSerializer {
    class CHIRSerializerImpl;
public:
    static void Serialize(const Package& package, const std::string filename, ToCHIR::Phase phase);
};

} // namespace Cangjie::CHIR

#endif
