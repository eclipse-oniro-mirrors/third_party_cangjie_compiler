// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements interpreter values.
 */

#include "cangjie/CHIR/Interpreter/InterpreterValue.h"

using namespace Cangjie::CHIR::Interpreter;

void IValUtils::Printer(const IVal& v, std::ostream& os)
{
    if (auto ui8 = IValUtils::GetIf<IUInt8>(&v)) {
        os << "uint8/" << ui8->content;
    } else if (auto ui16 = IValUtils::GetIf<IUInt16>(&v)) {
        os << "uint16/" << ui16->content;
    } else if (auto ui32 = IValUtils::GetIf<IUInt32>(&v)) {
        os << "uint32/" << ui32->content;
    } else if (auto ui64 = IValUtils::GetIf<IUInt64>(&v)) {
        os << "uint64/" << ui64->content;
    } else if (auto uinat = IValUtils::GetIf<IUIntNat>(&v)) {
        os << "uintNat/" << uinat->content;
    } else if (auto i8 = IValUtils::GetIf<IInt8>(&v)) {
        os << "int8/" << i8->content;
    } else if (auto i16 = IValUtils::GetIf<IInt16>(&v)) {
        os << "int16/" << i16->content;
    } else if (auto i32 = IValUtils::GetIf<IInt32>(&v)) {
        os << "int32/" << i32->content;
    } else if (auto i64 = IValUtils::GetIf<IInt64>(&v)) {
        os << "int64/" << i64->content;
    } else if (auto inat = IValUtils::GetIf<IIntNat>(&v)) {
        os << "intNat/" << inat->content;
    } else if (auto f16 = IValUtils::GetIf<IFloat16>(&v)) {
        os << "float16/" << f16->content;
    } else if (auto f32 = IValUtils::GetIf<IFloat32>(&v)) {
        os << "float32/" << f32->content;
    } else if (auto f64 = IValUtils::GetIf<IFloat64>(&v)) {
        os << "float64/" << f64->content;
    } else {
        // There is no reason to break this function in two parts,
        // other than code quality checks.
        PrintNonNumeric(v, os);
    }
}

std::string IValUtils::ToString(const IVal& v)
{
    std::stringstream ss;
    IValUtils::Printer(v, ss);
    return ss.str();
}

void IValUtils::PrintVector(const std::vector<IVal>& vec, std::ostream& os)
{
    os << "(";
    for (size_t i = 0; i < vec.size(); ++i) {
        Printer(vec[i], os);
        if (i != vec.size() - 1) {
            os << ", ";
        }
    }
    os << ")";
}

void IValUtils::PrintNonNumeric(const IVal& v, std::ostream& os = std::cout)
{
    if (auto c = IValUtils::GetIf<IRune>(&v)) {
        os << "char/" << c->content;
    } else if (auto b = IValUtils::GetIf<IBool>(&v)) {
        os << (b->content ? "bool/true" : "bool/false");
    } else if (IValUtils::GetIf<IUnit>(&v)) {
        os << "unit";
    } else if (IValUtils::GetIf<INullptr>(&v)) {
        os << "nullptr";
    } else if (auto p = IValUtils::GetIf<IPointer>(&v)) {
        os << "ptr(";
        os << "addr=" << p->content << "; ";
        Printer(*p->content, os);
        os << ")";
    } else if (auto tuple = IValUtils::GetIf<ITuple>(&v)) {
        os << "tuple/";
        PrintVector(tuple->content, os);
    } else if (auto array = IValUtils::GetIf<IArray>(&v)) {
        os << "array/";
        PrintVector(array->content, os);
    } else if (auto obj = IValUtils::GetIf<IObject>(&v)) {
        os << "object/classId=" << obj->classId << "/";
        PrintVector(obj->content, os);
    } else if (auto iFunc = IValUtils::GetIf<IFunc>(&v)) {
        os << "func/" << iFunc->content;
    } else {
        CJC_ABORT();
    }
}
