// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Type/EnumDef.h"

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/ToStringUtils.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Value.h"

using namespace Cangjie::CHIR;

bool EnumDef::IsAllCtorsTrivial() const
{
    // if enum all ctor does not have params, it is a trivial enum
    for (auto& ctor : ctors) {
        if (ctor.funcType->GetParamTypes().size() != 0) {
            return false;
        }
    }
    return true;
}

static std::string PrintParamTypes(const std::vector<Type*>& paramTypes)
{
    if (paramTypes.empty()) {
        return "";
    }
    std::string str;
    str += "(";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        str += paramTypes[i]->ToString();
        if (i != paramTypes.size() - 1) {
            str += ", ";
        }
    }
    str += ")";
    return str;
}

void EnumDef::PrintConstructor(std::stringstream& ss) const
{
    for (auto& ctor : ctors) {
        PrintIndent(ss);
        ss << ctor.name << PrintParamTypes(ctor.funcType->GetParamTypes()) << "\n";
    }
    ss << "\n";
}

void EnumDef::PrintAttrAndTitle(std::stringstream& ss) const
{
    ss << attributeInfo.ToString();
    if (!IsExhaustive()) {
        ss << "[nonExhaustive] ";
    }
    ss << CustomTypeKindToString(*this) << " " << GetIdentifier() << GenericDefArgsToString();
    PrintParent(ss);
}

std::string EnumDef::ToString() const
{
    /* [public][generic][...] enum XXX {      // loc: xxx, genericDecl: xxx
       ^^^^^^^^^^^^^^ attr    ^^^^^^^^^ title  ^^^^^^^^^^^^^^^^^^ comment
           constructor
           method
           vtable
       }
    */
    std::stringstream ss;
    PrintAttrAndTitle(ss);
    ss << " {";
    PrintComment(ss);
    ss << "\n";
    PrintConstructor(ss); // has a \n in the end
    PrintMethod(ss);      // has a \n in the end
    PrintVTable(ss);      // has a \n in the end
    ss << "}";
    return ss.str();
}

void EnumDef::AddCtor(EnumCtorInfo ctor)
{
    ctors.emplace_back(ctor);
}

size_t EnumDef::GetCtorSize() const
{
    return ctors.size();
}

std::vector<EnumCtorInfo> EnumDef::GetCtors() const
{
    return ctors;
}

void EnumDef::SetCtors(const std::vector<EnumCtorInfo>& items)
{
    ctors = items;
}

EnumCtorInfo EnumDef::GetCtor(size_t index) const
{
    CJC_ASSERT(ctors.size() > index);
    return ctors[index];
}

void EnumDef::SetType(CustomType& ty)
{
    CJC_ASSERT(ty.GetTypeKind() == Type::TypeKind::TYPE_ENUM);
    type = &ty;
}

EnumType* EnumDef::GetType() const
{
    return StaticCast<EnumType>(type);
}

bool EnumDef::IsExhaustive() const
{
    return !nonExhaustive;
}