// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Type/ClassDef.h"

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/ToStringUtils.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Value.h"
#include "cangjie/Utils/CheckUtils.h"

#include <iostream>
#include <sstream>

using namespace Cangjie::CHIR;

ClassDef::ClassDef(std::string srcCodeIdentifier, std::string identifier,
    std::string pkgName, bool isClass)
    : CustomTypeDef(srcCodeIdentifier, identifier, pkgName, CustomDefKind::TYPE_CLASS), isClass(isClass)
{
}

ClassDef* ClassDef::GetSuperClassDef() const
{
    return superClassTy ? superClassTy->GetClassDef() : nullptr;
}

void ClassDef::PrintAbstractMethod(std::stringstream& ss) const
{
    for (auto& method : abstractMethods) {
        PrintIndent(ss);
        ss << method.attributeInfo.ToString();
        ss << "func " << method.methodName << ": " << method.methodTy->ToString() << "\n";
    }
}

void ClassDef::SetSuperClassTy(ClassType& ty)
{
    superClassTy = &ty;
}

std::string ClassDef::ToString() const
{
    std::stringstream ss;
    PrintAttrAndTitle(ss);
    ss << " {";
    PrintComment(ss);
    ss << "\n";

    PrintLocalVar(ss);
    PrintStaticVar(ss);
    PrintMethod(ss);
    PrintAbstractMethod(ss);
    PrintVTable(ss);
    ss << "}";
    return ss.str();
}

bool ClassDef::IsAbstract() const
{
    return TestAttr(CHIR::Attribute::ABSTRACT);
}

bool ClassDef::IsInterface() const
{
    return !isClass;
}

bool ClassDef::IsClass() const
{
    return isClass;
}

void ClassDef::SetAnnotation(bool value)
{
    isAnnotation = value;
}

bool ClassDef::IsAnnotation() const
{
    return isAnnotation;
}

ClassType* ClassDef::GetSuperClassTy() const
{
    return superClassTy;
}

FuncBase* ClassDef::GetFinalizer() const
{
    for (auto m : methods) {
        if (m->GetFuncKind() == FuncKind::FINALIZER) {
            return m;
        }
    }
    return nullptr;
}

void ClassDef::AddAbstractMethod(AbstractMethodInfo methodInfo)
{
    abstractMethods.emplace_back(std::move(methodInfo));
}

std::vector<AbstractMethodInfo> ClassDef::GetAbstractMethods() const
{
    return abstractMethods;
}

void ClassDef::SetAbstractMethods(const std::vector<AbstractMethodInfo>& methods)
{
    abstractMethods = methods;
}

void ClassDef::SetType(CustomType& ty)
{
    CJC_ASSERT(ty.GetTypeKind() == Type::TypeKind::TYPE_CLASS);
    type = &ty;
}

ClassType* ClassDef::GetType() const
{
    return StaticCast<ClassType>(type);
}

void ClassDef::PrintComment(std::stringstream& ss) const
{
    CustomTypeDef::PrintComment(ss);
    AddCommaOrNot(ss);
    if (ss.str().empty()) {
        ss << " // ";
    }
    ss << "isAnnotation: " << BoolToString(isAnnotation);
}