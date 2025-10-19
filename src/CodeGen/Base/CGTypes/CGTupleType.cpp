// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "Base/CGTypes/CGTupleType.h"

#include "Base/CGTypes/CGStructType.h"
#include "CGContext.h"
#include "CGModule.h"
#include "Utils/CGUtils.h"
#include "cangjie/Option/Option.h"

namespace Cangjie::CodeGen {
namespace {
bool IsSized(CGModule& cgMod, const CHIR::TupleType& chirType)
{
    const auto memberVars = chirType.GetElementTypes();
    for (auto memberVar: memberVars) {
        if (memberVar->IsRef() || memberVar->IsCPointer() || memberVar->IsCFunc()) {
            continue;
        }
        if (!CGType::GetOrCreate(cgMod, memberVar)->GetSize()) {
            return false;
        }
    }
    return true;
}
}
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
void CGTupleType::CalculateSizeAndAlign()
{
    if (auto structType = llvm::dyn_cast<llvm::StructType>(llvmType)) {
        auto layOut = cgMod.GetLLVMModule()->getDataLayout();
        size = layOut.getTypeAllocSize(structType);
        align = layOut.getABITypeAlignment(structType);
    }
}

llvm::Type* CGTupleType::GenLLVMType()
{
    auto& llvmCtx = cgCtx.GetLLVMContext();
    auto& tupleType = StaticCast<const CHIR::TupleType&>(chirType);
    if (!IsSized(cgMod, tupleType)) {
        return llvm::Type::getInt8Ty(llvmCtx);
    }

    auto typeName = GetTypeQualifiedName(tupleType);
    llvmType = llvm::StructType::getTypeByName(llvmCtx, typeName);
    if (llvmType && cgCtx.IsGeneratedStructType(typeName)) {
        layoutType = llvm::cast<llvm::StructType>(llvmType);
        return llvmType;
    } else if (!llvmType) {
        llvmType = llvm::StructType::create(llvmCtx, typeName);
    }
    layoutType = llvm::cast<llvm::StructType>(llvmType);
    cgCtx.AddGeneratedStructType(typeName);

    std::vector<llvm::Type*> fieldTypes;
    for (auto argType : tupleType.GetElementTypes()) {
        auto cgType = CGType::GetOrCreate(cgMod, argType);
        (void)fieldTypes.emplace_back(cgType->GetLLVMType());
    }
    SetStructTypeBody(llvm::cast<llvm::StructType>(llvmType), fieldTypes);

    return llvmType;
}
#endif

void CGTupleType::GenContainedCGTypes()
{
    auto& tupleType = StaticCast<const CHIR::TupleType&>(chirType);
    for (auto& elemType : tupleType.GetElementTypes()) {
        (void)containedCGTypes.emplace_back(CGType::GetOrCreate(cgMod, elemType));
    }
}

llvm::Constant* CGTupleType::GenFieldsNumOfTypeInfo()
{
    auto fieldsNum = StaticCast<const CHIR::TupleType&>(chirType).GetElementTypes().size();
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(cgMod.GetLLVMContext()), fieldsNum);
}

llvm::Constant* CGTupleType::GenOffsetsOfTypeInfo()
{
    CJC_NULLPTR_CHECK(layoutType);
    return CGCustomType::GenOffsetsArray(cgMod, CGType::GetNameOfTypeInfoGV(chirType) + ".offsets", layoutType);
}

llvm::Constant* CGTupleType::GenFieldsOfTypeInfo()
{
    auto p0i8 = llvm::Type::getInt8PtrTy(cgMod.GetLLVMContext());
    std::vector<llvm::Constant*> fieldConstants;
    auto instanceMemberTypes = StaticCast<const CHIR::TupleType&>(chirType).GetElementTypes();
    for (auto type : instanceMemberTypes) {
        auto derefType = DeRef(*type);
        (void)fieldConstants.emplace_back(CGType::GetOrCreate(cgMod, derefType)->GetOrCreateTypeInfo());
    }

    if (fieldConstants.empty()) {
        return llvm::Constant::getNullValue(p0i8);
    }

    auto typeInfoPtrTy = CGType::GetOrCreateTypeInfoPtrType(cgMod.GetLLVMContext());
    auto typeOfFieldsGV = llvm::ArrayType::get(typeInfoPtrTy, fieldConstants.size());
    auto typeInfoOfFields = llvm::cast<llvm::GlobalVariable>(
        cgMod.GetLLVMModule()->getOrInsertGlobal(CGType::GetNameOfTypeInfoGV(chirType) + ".fields", typeOfFieldsGV));
    typeInfoOfFields->setInitializer(llvm::ConstantArray::get(typeOfFieldsGV, fieldConstants));
    typeInfoOfFields->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    typeInfoOfFields->addAttribute(CJTI_FIELDS_ATTR);
    return llvm::ConstantExpr::getBitCast(typeInfoOfFields, p0i8);
}

llvm::Constant* CGTupleType::GenSourceGenericOfTypeInfo()
{
    return CGType::GenSourceGenericOfTypeInfo();
}

llvm::Constant* CGTupleType::GenTypeArgsNumOfTypeInfo()
{
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(cgMod.GetLLVMContext()),
        StaticCast<const CHIR::TupleType&>(chirType).GetElementTypes().size());
}

llvm::Constant* CGTupleType::GenTypeArgsOfTypeInfo()
{
    auto genericArgs = StaticCast<const CHIR::TupleType&>(chirType).GetElementTypes();
    auto typeInfoPtrTy = CGType::GetOrCreateTypeInfoPtrType(cgMod.GetLLVMContext());
    auto p0i8 = llvm::Type::getInt8PtrTy(cgMod.GetLLVMContext());
    if (genericArgs.empty()) {
        return llvm::ConstantPointerNull::get(p0i8);
    }

    std::vector<llvm::Constant*> constants;
    for (auto arg : genericArgs) {
        (void)constants.emplace_back(CGType::GetOrCreate(cgMod, DeRef(*arg))->GetOrCreateTypeInfo());
    }

    auto typeOfGenericArgsGV = llvm::ArrayType::get(typeInfoPtrTy, constants.size());
    auto typeInfoOfGenericArgs = llvm::cast<llvm::GlobalVariable>(cgMod.GetLLVMModule()->getOrInsertGlobal(
            CGType::GetNameOfTypeInfoGV(chirType) + ".typeArgs", typeOfGenericArgsGV));
    typeInfoOfGenericArgs->setInitializer(llvm::ConstantArray::get(typeOfGenericArgsGV, constants));
    typeInfoOfGenericArgs->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    typeInfoOfGenericArgs->addAttribute(CJTI_TYPE_ARGS_ATTR);
    return llvm::ConstantExpr::getBitCast(typeInfoOfGenericArgs, p0i8);
}

llvm::Constant* CGTupleType::GenSuperOfTypeInfo()
{
    return CGType::GenSuperOfTypeInfo();
}
} // namespace Cangjie::CodeGen
