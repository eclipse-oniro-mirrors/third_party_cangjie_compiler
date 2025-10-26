// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_CHIR_SERIALIZER_IMPL_H
#define CANGJIE_CHIR_SERIALIZER_IMPL_H

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include <flatbuffers/PackageFormat_generated.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <deque>
#include <fstream>
#include <iostream>
#include <queue>

#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/CHIR/CHIRContext.h"
#include "cangjie/CHIR/Package.h"
#include "cangjie/CHIR/Serializer/CHIRSerializer.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Value.h"

namespace Cangjie::CHIR {

class CHIRSerializer::CHIRSerializerImpl {
public:
    explicit CHIRSerializerImpl(const Package& package) : package(package){};

    // Utility
    void Save(const std::string& filename, ToCHIR::Phase phase);
    void Initialize();
    void Dispatch();

private:
    const Package& package;

    flatbuffers::FlatBufferBuilder builder;
    std::deque<const Value*> valueQueue;
    std::queue<const Type*> typeQueue;
    std::queue<const Expression*> exprQueue;
    std::deque<const CustomTypeDef*> defQueue;

    uint32_t typeCount = 0;
    uint32_t valueCount = 0;
    uint32_t exprCount = 0;
    uint32_t defCount = 0;

    // Id maps
    std::unordered_map<const Type*, uint32_t> type2Id{{nullptr, 0}};
    std::unordered_map<const Value*, uint32_t> value2Id{{nullptr, 0}};
    std::unordered_map<const Expression*, uint32_t> expr2Id{{nullptr, 0}};
    std::unordered_map<const CustomTypeDef*, uint32_t> def2Id{{nullptr, 0}};

    // Kind Indicators
    std::vector<uint8_t> typeKind{};
    std::vector<uint8_t> valueKind{};
    std::vector<uint8_t> exprKind{};
    std::vector<uint8_t> defKind{};

    // Containers
    std::vector<flatbuffers::Offset<void>> allType{};
    std::vector<flatbuffers::Offset<void>> allValue{};
    std::vector<flatbuffers::Offset<void>> allExpression{};
    std::vector<flatbuffers::Offset<void>> allCustomTypeDef{};

    // Serializers
    template <typename FBT, typename T> flatbuffers::Offset<FBT> Serialize(const T& obj);
    template <typename FBT, typename T> std::vector<flatbuffers::Offset<FBT>> SerializeVec(const std::vector<T>& vec);
    template <typename FBT, typename T>
    std::vector<flatbuffers::Offset<FBT>> SerializeSetToVec(const std::unordered_set<T>& set) const;
    std::vector<flatbuffers::Offset<PackageFormat::VTableElement>> SerializeVTable(const VTableType& obj);
    // Dispatchers
    template <typename T> flatbuffers::Offset<void> Dispatch(const T& obj);

    // Fetch ID
    template <typename T> uint32_t GetId(const T* obj);
    template <typename T, typename E> std::vector<uint32_t> GetId(std::vector<E*> vec);
    template <typename T, typename E> std::vector<uint32_t> GetId(std::vector<Ptr<E>> vec);
    template <typename T, typename E> std::vector<uint32_t> GetId(const std::unordered_set<E*>& set) const;
    // other to save
    unsigned packageInitFunc{};
    unsigned packageLiteralInitFunc{};
    uint32_t maxImportedValueId = 0;
    uint32_t maxImportedStructId = 0;
    uint32_t maxImportedClassId = 0;
    uint32_t maxImportedEnumId = 0;
    uint32_t maxImportedExtendId = 0;
};
} // namespace Cangjie::CHIR

#endif
