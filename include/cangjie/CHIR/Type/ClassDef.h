// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_CLASS_H
#define CANGJIE_CHIR_CLASS_H

#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Value.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Cangjie::CHIR {
class ClassDef : public CustomTypeDef {
    friend class CustomDefTypeConverter;
public:
    // ===--------------------------------------------------------------------===//
    // Base Infomation
    // ===--------------------------------------------------------------------===//
    ClassType* GetType() const override;
    void SetType(CustomType& ty) override;
    
    bool IsAbstract() const;
    bool IsClass() const;
    bool IsInterface() const;

    std::string ToString() const override;

    /**
     * @brief Whether this class is user defined annotation.
     *
     * @return return true for classes that are marked with the @Annotation annotation.
     */
    bool IsAnnotation() const;
    void SetAnnotation(bool value);
    // ===--------------------------------------------------------------------===//
    // Super Parent
    // ===--------------------------------------------------------------------===//
    ClassType* GetSuperClassTy() const;
    ClassDef* GetSuperClassDef() const;
    void SetSuperClassTy(ClassType& ty);

    // ===--------------------------------------------------------------------===//
    // Member Function
    // ===--------------------------------------------------------------------===//
    void AddAbstractMethod(AbstractMethodInfo methodInfo);
    std::vector<AbstractMethodInfo> GetAbstractMethods() const;
    void SetAbstractMethods(const std::vector<AbstractMethodInfo>& methods);
    
    FuncBase* GetFinalizer() const;

protected:
    void PrintComment(std::stringstream& ss) const override;
    
private:
    explicit ClassDef(std::string srcCodeIdentifier, std::string identifier,
        std::string pkgName, bool isClass);
    ~ClassDef() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;
    void PrintAbstractMethod(std::stringstream& ss) const;

    bool isClass = false;           // class or interface
    bool isAnnotation = false;      // whether the class is modified by @Annotation
    ClassType* superClassTy = nullptr;
    std::vector<AbstractMethodInfo> abstractMethods;
};
} // namespace Cangjie::CHIR

#endif
