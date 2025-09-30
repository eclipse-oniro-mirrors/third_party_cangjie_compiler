// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "CHIRDeserializerImpl.h"

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/Type/ClassDef.h"
#include "cangjie/CHIR/Type/EnumDef.h"
#include "cangjie/CHIR/Type/StructDef.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Utils/ICEUtil.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/GeneratedFromForIn.h"
#include "cangjie/CHIR/Serializer/CHIRDeserializer.h"

using namespace Cangjie::CHIR;

// explicit specialization
template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::EnumDef* buffer, EnumDef& obj);
template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::StructDef* buffer, StructDef& obj);
template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::ClassDef* buffer, ClassDef& obj);

// =========================== Generic Deserializer ==============================

void CHIRDeserializer::Deserialize(const std::string& fileName, Cangjie::CHIR::CHIRBuilder& chirBuilder)
{
    CHIRDeserializerImpl deserializer(chirBuilder);
    auto buffer = deserializer.Read(fileName);
    if (!buffer) {
        return;
    }
    deserializer.Run(PackageFormat::GetCHIRPackage(buffer));
}

template <typename T, typename FBT>
std::vector<T> CHIRDeserializer::CHIRDeserializerImpl::Create(const flatbuffers::Vector<FBT>* vec)
{
    std::vector<T> retval;
    for (auto obj : *vec) {
        retval.emplace_back(Create<T>(obj));
    }
    return retval;
}

template <typename T>
std::vector<T*> CHIRDeserializer::CHIRDeserializerImpl::GetValue(const flatbuffers::Vector<uint32_t>* vec)
{
    std::vector<T*> retval;
    for (auto obj : *vec) {
        if (auto elem = GetValue<T>(obj)) {
            retval.emplace_back(elem);
        }
    }
    return retval;
}

template <typename T>
std::vector<T*> CHIRDeserializer::CHIRDeserializerImpl::GetType(const flatbuffers::Vector<uint32_t>* vec)
{
    std::vector<T*> retval;
    for (auto obj : *vec) {
        if (auto elem = GetType<T>(obj)) {
            retval.emplace_back(elem);
        }
    }
    return retval;
}

template <typename T>
std::vector<T*> CHIRDeserializer::CHIRDeserializerImpl::GetExpression(const flatbuffers::Vector<uint32_t>* vec)
{
    std::vector<T*> retval;
    for (auto obj : *vec) {
        if (auto elem = GetExpression<T>(obj)) {
            retval.emplace_back(elem);
        }
    }
    return retval;
}

template <typename T>
std::vector<T*> CHIRDeserializer::CHIRDeserializerImpl::GetCustomTypeDef(const flatbuffers::Vector<uint32_t>* vec)
{
    std::vector<T*> retval;
    for (auto obj : *vec) {
        if (auto elem = GetCustomTypeDef<T>(obj)) {
            retval.emplace_back(elem);
        }
    }
    return retval;
}

// =========================== Helper Deserializer ==============================

namespace {
AttributeInfo CreateAttr(const uint64_t attrs)
{
    return AttributeInfo(attrs);
}
} // namespace

template <> Position CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::Pos* obj)
{
    return Position{static_cast<unsigned>(obj->line()), static_cast<unsigned>(obj->column())};
}

template <> DebugLocation CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::DebugLocation* obj)
{
    auto filePath = obj->filePath()->str();
    auto fileId = obj->fileId();
    auto beginPos = Create<Position>(obj->beginPos());
    auto endPos = Create<Position>(obj->endPos());
    auto scope = std::vector<int>(obj->scope()->begin(), obj->scope()->end());

    if (builder.GetChirContext().GetSourceFileName(fileId) == INVALID_NAME) {
        builder.GetChirContext().RegisterSourceFileName(fileId, filePath);
    }
    // DebugLocation stores a string pointer, so here can't just pass filePath.
    return DebugLocation(builder.GetChirContext().GetSourceFileName(fileId), fileId, beginPos, endPos, scope);
}

template <> AnnoInfo CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::AnnoInfo* obj)
{
    auto mangledName = obj->mangledName()->str();
    return AnnoInfo{mangledName};
}

template <> MemberVarInfo CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::MemberVarInfo* obj)
{
    auto name = obj->name()->str();
    auto rawMangledName = obj->rawMangledName()->str();
    auto type = GetType<Type>(obj->type());
    auto attributeInfo = CreateAttr(obj->attributes());
    auto loc = Create<DebugLocation>(obj->loc());
    auto annoInfo = Create<AnnoInfo>(obj->annoInfo());
    return MemberVarInfo{name, rawMangledName, type, attributeInfo, loc, annoInfo};
}

template <> EnumCtorInfo CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::EnumCtorInfo* obj)
{
    auto name = obj->identifier()->str();
    auto mangledName = obj->mangledName()->str();
    auto funcType = GetType<FuncType>(obj->funcType());
    return EnumCtorInfo{name, mangledName, funcType};
}

template <>
AbstractMethodParam CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::AbstractMethodParam* obj)
{
    auto paramName = obj->paramName()->str();
    auto paramType = GetType<Type>(obj->paramType());
    auto annoInfo = Create<AnnoInfo>(obj->annoInfo());
    return AbstractMethodParam{paramName, paramType, annoInfo};
}

template <>
AbstractMethodInfo CHIRDeserializer::CHIRDeserializerImpl::Create(const PackageFormat::AbstractMethodInfo* obj)
{
    auto name = obj->methodName()->str();
    auto mangledName = obj->mangledName()->str();
    auto methodTy = GetType<Type>(obj->methodType());
    auto paramInfos = Create<AbstractMethodParam>(obj->paramsInfo());
    auto attributeInfo = CreateAttr(obj->attributes());
    auto annoInfo = Create<AnnoInfo>(obj->annoInfo());
    // UG need fix
    return AbstractMethodInfo{
        name, mangledName, methodTy, paramInfos, attributeInfo, annoInfo, std::vector<GenericType*>{}, false, nullptr};
}

// =========================== Custom Type Define Deserializer ==============================

template <> EnumDef* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::EnumDef* obj)
{
    auto srcCodeIdentifier = obj->base()->srcCodeIdentifier()->str();
    auto identifier = obj->base()->identifier()->str();
    auto packageName = obj->base()->packageName()->str();
    auto attrs = CreateAttr(obj->base()->attributes());
    auto imported = attrs.TestAttr(CHIR::Attribute::IMPORTED);
    auto genericTypeParams = GetType<GenericType>(obj->base()->genericTypeParams());
    auto result = builder.CreateEnum(
        DebugLocation(), srcCodeIdentifier, identifier, packageName, imported, obj->nonExhaustive());
    result->AppendAttributeInfo(attrs);

    return result;
}

template <> StructDef* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::StructDef* obj)
{
    auto srcCodeIdentifier = obj->base()->srcCodeIdentifier()->str();
    auto identifier = obj->base()->identifier()->str();
    auto packageName = obj->base()->packageName()->str();
    auto attrs = CreateAttr(obj->base()->attributes());
    auto imported = attrs.TestAttr(CHIR::Attribute::IMPORTED);
    auto genericTypeParams = GetType<GenericType>(obj->base()->genericTypeParams());
    auto result =
        builder.CreateStruct(DebugLocation(), srcCodeIdentifier, identifier, packageName, imported);
    result->AppendAttributeInfo(attrs);
    return result;
}

template <> ClassDef* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ClassDef* obj)
{
    auto srcCodeIdentifier = obj->base()->srcCodeIdentifier()->str();
    auto identifier = obj->base()->identifier()->str();
    auto packageName = obj->base()->packageName()->str();
    auto isClass = obj->kind() == PackageFormat::ClassDefKind::ClassDefKind_CLASS;
    auto attrs = CreateAttr(obj->base()->attributes());
    auto imported = attrs.TestAttr(CHIR::Attribute::IMPORTED);
    auto genericTypeParams = GetType<GenericType>(obj->base()->genericTypeParams());
    auto result =
        builder.CreateClass(DebugLocation(), srcCodeIdentifier, identifier, packageName, !isClass, imported);
    result->AppendAttributeInfo(attrs);
    // UG might need fix here
    return result;
}

// =========================== Type Deserializer ==============================

template <>
RuneType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::RuneType* obj)
{
    return builder.GetType<RuneType>();
}

template <>
BooleanType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::BooleanType* obj)
{
    return builder.GetType<BooleanType>();
}

template <>
UnitType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::UnitType* obj)
{
    return builder.GetType<UnitType>();
}

template <>
NothingType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::NothingType* obj)
{
    return builder.GetType<NothingType>();
}

template <> TupleType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::TupleType* obj)
{
    auto argTys = GetType<Type>(obj->base()->argTys());
    return builder.GetType<TupleType>(argTys);
}

template <> ClosureType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ClosureType* obj)
{
    auto argTys = obj->base()->argTys();
    auto funcTy = GetType<Type>(argTys->Get(static_cast<size_t>(ClosureType::Element::FUNC_INDEX)));
    auto envTy = GetType<Type>(argTys->Get(static_cast<size_t>(ClosureType::Element::ENV_INDEX)));
    return builder.GetType<ClosureType>(funcTy, envTy);
}

template <> RawArrayType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RawArrayType* obj)
{
    auto elemTy = GetType<Type>(obj->base()->argTys()->Get(0));
    auto dims = obj->dims();
    return builder.GetType<RawArrayType>(elemTy, static_cast<unsigned>(dims));
}

template <> VArrayType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::VArrayType* obj)
{
    auto elemTy = GetType<Type>(obj->base()->argTys()->Get(0));
    auto size = obj->size();
    return builder.GetType<VArrayType>(elemTy, size);
}

template <> FuncType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::FuncType* obj)
{
    auto argTys = obj->base()->argTys();
    auto retTy = GetType<Type>(argTys->Get(argTys->size() - 1));
    auto hasVarLenParam = obj->hasVarArg();
    auto isCFuncType = obj->isCFuncType();
    std::vector<Type*> paramTys;
    for (size_t i = 0; i < argTys->size() - 1; ++i) {
        paramTys.emplace_back(GetType<Type>(argTys->Get(static_cast<unsigned>(i))));
    }
    return builder.GetType<FuncType>(paramTys, retTy, hasVarLenParam, isCFuncType);
}

template <>
CStringType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::CStringType* obj)
{
    return builder.GetType<CStringType>();
}

template <> CPointerType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::CPointerType* obj)
{
    auto elemTy = GetType<Type>(obj->base()->argTys()->Get(0));
    return builder.GetType<CPointerType>(elemTy);
}

template <> GenericType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::GenericType* obj)
{
    auto identifier = obj->identifier()->str();
    // UG Need Fix Here
    auto genericType = builder.GetType<GenericType>(identifier, identifier);
    genericTypeConfig.emplace_back(genericType, obj);
    return genericType;
}

template <> RefType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RefType* obj)
{
    auto baseType = GetType<Type>(obj->base()->argTys()->Get(0));
    return builder.GetType<RefType>(baseType);
}

template <>
VoidType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize([[maybe_unused]] const PackageFormat::VoidType* obj)
{
    return builder.GetType<VoidType>();
}

// =========================== Custom Type Deserializer ==============================

template <> EnumType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::EnumType* obj)
{
    auto def = GetCustomTypeDef<EnumDef>(obj->base()->customTypeDef());
    auto argTys = GetType<Type>(obj->base()->base()->argTys());
    return builder.GetType<EnumType>(def, argTys);
}

template <> StructType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::StructType* obj)
{
    auto def = GetCustomTypeDef<StructDef>(obj->base()->customTypeDef());
    auto argTys = GetType<Type>(obj->base()->base()->argTys());
    return builder.GetType<StructType>(def, argTys);
}

template <> ClassType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ClassType* obj)
{
    auto def = GetCustomTypeDef<ClassDef>(obj->base()->customTypeDef());
    auto argTys = GetType<Type>(obj->base()->base()->argTys());
    return builder.GetType<ClassType>(def, argTys);
}

// =========================== Numeric Type Deserializer ==============================

template <> IntType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::IntType* obj)
{
    auto kind = Type::TypeKind(obj->base()->base()->kind());
    return builder.GetType<IntType>(kind);
}

template <> FloatType* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::FloatType* obj)
{
    auto kind = Type::TypeKind(obj->base()->base()->kind());
    return builder.GetType<FloatType>(kind);
}

// =========================== Value Deserializer ==============================

template <> BoolLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::BoolLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto val = obj->val();
    return builder.CreateLiteralValue<BoolLiteral>(type, val);
}

template <> RuneLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RuneLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto val = obj->val();
    return builder.CreateLiteralValue<RuneLiteral>(type, static_cast<char32_t>(val));
}

template <> StringLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::StringLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto val = obj->val()->str();
    return builder.CreateLiteralValue<StringLiteral>(type, val, obj->isJString());
}

template <> IntLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::IntLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto val = obj->val();
    return builder.CreateLiteralValue<IntLiteral>(type, val);
}

template <> FloatLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::FloatLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto val = obj->val();
    return builder.CreateLiteralValue<FloatLiteral>(type, val);
}

template <> UnitLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::UnitLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    return builder.CreateLiteralValue<UnitLiteral>(type);
}

template <> NullLiteral* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::NullLiteral* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    return builder.CreateLiteralValue<NullLiteral>(type);
}

template <> Func* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Func* obj)
{
    auto funcTy = GetType<FuncType>(obj->base()->type());
    auto identifier = obj->base()->identifier()->str();
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto packageName = obj->packageName()->str();
    auto genericTypeParams = GetType<GenericType>(obj->genericTypeParams());
    return builder.CreateFunc(
        DebugLocation(), funcTy, identifier, srcCodeIdentifier, identifier, packageName, genericTypeParams);
}

template <> BlockGroup* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::BlockGroup* obj)
{
    BlockGroup* blockGroup = nullptr;
    if (auto ownedFunc = GetValue<Func>(obj->ownedFunc())) {
        blockGroup = builder.CreateBlockGroup(*ownedFunc);
    } else if (auto ownedLambda = GetExpression<Lambda>(obj->ownedLambda())) {
        CJC_NULLPTR_CHECK(ownedLambda->GetParentFunc());
        blockGroup = builder.CreateBlockGroup(*ownedLambda->GetParentFunc());
    } else {
        CJC_ABORT();
    }
    if (auto ownedLambda = GetExpression<Lambda>(obj->ownedLambda())) {
        ownedLambda->InitBody(*blockGroup);
    } else if (auto ownedFunc = GetValue<Func>(obj->ownedFunc())) {
        blockGroup->SetOwnerFunc(ownedFunc);
    } else {
        InternalError("deserialize BlockGroup failed.");
    }
    return blockGroup;
}

template <> Block* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Block* obj)
{
    auto parentGroup = GetValue<BlockGroup>(obj->parentGroup());
    return builder.CreateBlock(parentGroup);
}

template <> Parameter* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Parameter* obj)
{
    CJC_ASSERT(obj->base()->kind() == PackageFormat::ValueKind_PARAMETER);
    auto type = GetType<Type>(obj->base()->type());
    Parameter* result = nullptr;
    if (auto ownedFunc = GetValue<Func>(obj->ownedFunc())) {
        result = builder.CreateParameter(type, INVALID_LOCATION, *ownedFunc);
    } else if (auto ownedLambda = GetExpression<Lambda>(obj->ownedLambda())) {
        result = builder.CreateParameter(type, INVALID_LOCATION, *ownedLambda);
    } else {
        CJC_ABORT();
    }
    result->SetAnnoInfo(Create<AnnoInfo>(obj->annoInfo()));
    StaticCast<Value*>(result)->identifier = obj->base()->identifier()->str();
    return result;
}

template <> LocalVar* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::LocalVar* obj)
{
    auto associatedExpr = GetExpression<Expression>(obj->associatedExpr());
    CJC_NULLPTR_CHECK(associatedExpr);
    auto result = associatedExpr->GetResult();
    return result;
}

template <> GlobalVar* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::GlobalVar* obj)
{
    auto type = GetType<RefType>(obj->base()->type());
    auto identifier = obj->base()->identifier()->str();
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto packageName = obj->packageName()->str();
    auto rawMangledName = obj->rawMangledName()->str();
    auto attrs = CreateAttr(obj->base()->attributes());
    auto result = builder.CreateGlobalVar(DebugLocation(), type, identifier, srcCodeIdentifier, rawMangledName,
        packageName);
    result->AppendAttributeInfo(attrs);
    return result;
}

template <> ImportedFunc* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ImportedFunc* obj)
{
    auto type = GetType<Type>(obj->base()->base()->type());
    auto identifier = obj->base()->base()->identifier()->str();
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto packageName = obj->base()->packageName()->str();
    auto genericTypeParams = GetType<GenericType>(obj->genericTypeParams());
    auto attrs = CreateAttr(obj->base()->base()->attributes());
    auto importedFunc = builder.CreateImportedVarOrFunc<ImportedFunc>(type, identifier, srcCodeIdentifier, identifier,
        packageName, genericTypeParams);

    // Object configuration
    importedFunc->AppendAttributeInfo(attrs);
    importedFunc->SetFuncKind(static_cast<FuncKind>(obj->funcKind()));

    importedFunc->SetRawMangledName(obj->rawMangledName()->str());
    importedFunc->SetParamInfo(Create<AbstractMethodParam>(obj->paramInfo()));
    // UG FIX nullptr
    importedFunc->SetAnnoInfo(Create<AnnoInfo>(obj->annoInfo()));
    importedFunc->SetFastNative(obj->isFastNative());
    importedFunc->SetCFFIWrapper(obj->isCFFIWrapper());
    return importedFunc;
}

template <> ImportedVar* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ImportedVar* obj)
{
    auto type = GetType<RefType>(obj->base()->base()->type());
    auto identifier = obj->base()->base()->identifier()->str();
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto packageName = obj->base()->packageName()->str();
    auto attrs = CreateAttr(obj->base()->base()->attributes());
    auto importedVar = builder.CreateImportedVarOrFunc<ImportedVar>(
        type, identifier, srcCodeIdentifier, identifier, packageName);
    // Object configuration
    importedVar->AppendAttributeInfo(attrs);
    importedVar->SetAnnoInfo(Create<AnnoInfo>(obj->annoInfo()));
    return importedVar;
}

// =========================== Expression Deserializer ==============================

template <>
UnaryExpression* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::UnaryExpression* obj)
{
    auto kind = ExprKind(obj->base()->kind());
    auto operand = GetValue<Value>(obj->base()->operands()->Get(0));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    auto ofs = OverflowStrategy(obj->overflowStrategy());
    return builder.CreateExpression<UnaryExpression>(resultTy, kind, operand, ofs, parentBlock);
}

template <>
BinaryExpression* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::BinaryExpression* obj)
{
    auto kind = ExprKind(obj->base()->kind());
    auto lhs = GetValue<Value>(obj->base()->operands()->Get(0));
    auto rhs = GetValue<Value>(obj->base()->operands()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    auto ofs = OverflowStrategy(obj->overflowStrategy());
    return builder.CreateExpression<BinaryExpression>(resultTy, kind, lhs, rhs, ofs, parentBlock);
}

template <> Constant* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Constant* obj)
{
    auto val = static_cast<LiteralValue*>(GetValue<Value>(obj->base()->operands()->Get(0)));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    if (val->IsBoolLiteral()) {
        auto litVal = static_cast<BoolLiteral*>(val);
        return builder.CreateConstantExpression<BoolLiteral>(resultTy, parentBlock, litVal->GetVal());
    } else if (val->IsFloatLiteral()) {
        auto litVal = static_cast<FloatLiteral*>(val);
        return builder.CreateConstantExpression<FloatLiteral>(resultTy, parentBlock, litVal->GetVal());
    } else if (val->IsIntLiteral()) {
        auto litVal = static_cast<IntLiteral*>(val);
        return builder.CreateConstantExpression<IntLiteral>(resultTy, parentBlock, litVal->GetUnsignedVal());
    } else if (val->IsNullLiteral()) {
        return builder.CreateConstantExpression<NullLiteral>(resultTy, parentBlock);
    } else if (val->IsRuneLiteral()) {
        auto litVal = static_cast<RuneLiteral*>(val);
        return builder.CreateConstantExpression<RuneLiteral>(resultTy, parentBlock, litVal->GetVal());
    } else if (val->IsStringLiteral()) {
        auto litVal = static_cast<StringLiteral*>(val);
        return builder.CreateConstantExpression<StringLiteral>(resultTy, parentBlock, litVal->GetVal(), false);
    } else if (val->IsUnitLiteral()) {
        return builder.CreateConstantExpression<UnitLiteral>(resultTy, parentBlock);
    } else {
        CJC_ABORT();
    }
    return nullptr;
}

template <> Allocate* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Allocate* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto targetType = GetType<Type>(obj->targetType());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Allocate>(resultTy, targetType, parentBlock);
}

template <> Cangjie::CHIR::Load* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Load* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto location = GetValue<Value>(obj->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Cangjie::CHIR::Load>(resultTy, location, parentBlock);
}

template <> Store* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Store* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto value = GetValue<Value>(obj->base()->operands()->Get(0));
    auto location = GetValue<Value>(obj->base()->operands()->Get(1));
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Store>(resultTy, value, location, parentBlock);
}

template <> GetElementRef* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::GetElementRef* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto location = GetValue<Value>(obj->base()->operands()->Get(0));
    auto path = std::vector<uint64_t>(obj->path()->begin(), obj->path()->end());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<GetElementRef>(resultTy, location, path, parentBlock);
}

template <>
StoreElementRef* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::StoreElementRef* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto value = GetValue<Value>(obj->base()->operands()->Get(0));
    auto location = GetValue<Value>(obj->base()->operands()->Get(1));
    auto path = std::vector<uint64_t>(obj->path()->begin(), obj->path()->end());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<StoreElementRef>(resultTy, value, location, path, parentBlock);
}

template <> Apply* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Apply* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto operands = GetValue<Value>(obj->base()->operands());
    auto callee = operands[0];
    auto args = std::vector<Value*>(operands.begin() + 1, operands.end());
    auto resultTy = GetType<Type>(obj->base()->resultTy());

    auto genericArgs = GetType<Type>(obj->genericArgs());
    std::vector<Ptr<Type>> genericInfo;
    std::transform(genericArgs.begin(), genericArgs.end(), std::back_inserter(genericInfo),
        [](Type* type) { return Ptr<Type>(type); });
    // UG FIX nullptr
    auto result = builder.CreateExpression<Apply>(resultTy, callee, args, parentBlock);
    result->SetInstantiatedArgTypes(genericInfo);

    result->SetSuperCall(obj->isSuperCall());
    return result;
}

template <> Invoke* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Invoke* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto operands = GetValue<Value>(obj->base()->operands());
    auto object = operands[0];
    auto args = std::vector<Value*>(operands.begin() + 1, operands.end());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    // UG need fix here
    return builder.CreateExpression<Invoke>(resultTy, object, args, InvokeCalleeInfo{}, parentBlock);
}

template <> InvokeStatic* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::InvokeStatic* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto args = GetValue<Value>(obj->base()->operands());
    auto rtti = args[0];
    args.erase(args.begin());
    // UG need fix here
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<InvokeStatic>(resultTy, rtti, args, InvokeCalleeInfo{}, parentBlock);
}

template <>
InvokeStaticWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(
    const PackageFormat::InvokeStaticWithException* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto args = GetValue<Value>(obj->base()->base()->operands());
    auto rtti = args[0];
    args.erase(args.begin());
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    // need fix
    return builder.CreateExpression<InvokeStaticWithException>(
        resultTy, rtti, args, InvokeCalleeInfo{}, sucBlock, errBlock, parentBlock);
}

template <> TypeCast* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::TypeCast* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto operand = GetValue<Value>(obj->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    auto ofs = OverflowStrategy(obj->overflowStrategy());
    return builder.CreateExpression<TypeCast>(resultTy, operand, ofs, parentBlock);
}

template <> InstanceOf* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::InstanceOf* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto operand = GetValue<Value>(obj->base()->operands()->Get(0));
    auto targetType = GetType<Type>(obj->targetType());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<InstanceOf>(resultTy, operand, targetType, parentBlock);
}

template <> Box* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Box* expr)
{
    auto parentBlock = GetValue<Block>(expr->base()->parentBlock());
    auto operand = GetValue<Value>(expr->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(expr->base()->resultTy());
    return builder.CreateExpression<Box>(resultTy, operand, parentBlock);
}

template <> UnBox* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::UnBox* expr)
{
    auto parentBlock = GetValue<Block>(expr->base()->parentBlock());
    auto operand = GetValue<Value>(expr->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(expr->base()->resultTy());
    return builder.CreateExpression<UnBox>(resultTy, operand, parentBlock);
}
template <> Tuple* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Tuple* obj)
{
    auto operands = GetValue<Value>(obj->base()->operands());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Tuple>(resultTy, operands, parentBlock);
}

template <> Field* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Field* obj)
{
    auto val = GetValue<Value>(obj->base()->operands()->Get(0));
    auto indexes = std::vector<uint64_t>(obj->path()->begin(), obj->path()->end());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Field>(resultTy, val, indexes, parentBlock);
}

template <>
RawArrayAllocate* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RawArrayAllocate* obj)
{
    auto size = GetValue<Value>(obj->base()->operands()->Get(0));
    auto elementType = GetType<Type>(obj->elementType());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<RawArrayAllocate>(resultTy, elementType, size, parentBlock);
}

template <>
RawArrayLiteralInit* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RawArrayLiteralInit* obj)
{
    auto operands = GetValue<Value>(obj->base()->operands());
    auto raw = Ptr(operands[0]);
    auto elements = std::vector<Value*>(operands.begin() + 1, operands.end());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<RawArrayLiteralInit>(resultTy, raw, elements, parentBlock);
}

template <>
RawArrayInitByValue* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RawArrayInitByValue* obj)
{
    auto raw = GetValue<Value>(obj->base()->operands()->Get(0));
    auto size = GetValue<Value>(obj->base()->operands()->Get(1));
    auto initVal = GetValue<Value>(obj->base()->operands()->Get(2));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<RawArrayInitByValue>(resultTy, raw, size, initVal, parentBlock);
}

template <> VArray* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::VArray* obj)
{
    auto operands = GetValue<Value>(obj->base()->operands());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<VArray>(resultTy, operands, parentBlock);
}

template <> VArrayBuilder* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::VArrayBd* obj)
{
    auto size = GetValue<Value>(obj->base()->operands()->Get(0));
    auto item = GetValue<Value>(obj->base()->operands()->Get(1));
    auto initFunc = GetValue<Value>(obj->base()->operands()->Get(2));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<VArrayBuilder>(resultTy, size, item, initFunc, parentBlock);
}

template <> GetException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::GetException* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<GetException>(resultTy, parentBlock);
}

template <> Intrinsic* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Intrinsic* obj)
{
    auto kind = CHIR::IntrinsicKind(obj->intrinsicKind());
    auto args = GetValue<Value>(obj->base()->operands());
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    auto intrinsic = builder.CreateExpression<Intrinsic>(resultTy, kind, args, parentBlock);
    std::vector<Ptr<Type>> genericTypes;
    auto genericTypeInfo = GetType<Type>(obj->genericArgs());
    std::transform(genericTypeInfo.begin(), genericTypeInfo.end(), std::back_inserter(genericTypes),
        [](Type* type) { return Ptr<Type>(type); });
    intrinsic->SetGenericTypeInfo(genericTypes);
    return intrinsic;
}

template <> If* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::If* obj)
{
    auto cond = GetValue<Value>(obj->base()->operands()->Get(0));
    auto thenBody = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(0));
    auto elseBody = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<If>(resultTy, cond, thenBody, elseBody, parentBlock);
}

template <> Loop* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Loop* obj)
{
    auto loopBody = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(0));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Loop>(resultTy, loopBody, parentBlock);
}

template <> ForInRange* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ForInRange* obj)
{
    auto inductionVar = GetValue<Value>(obj->base()->operands()->Get(0));
    auto loopCondVar = GetValue<Value>(obj->base()->operands()->Get(1));
    auto body = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(0));
    auto latch = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(1));
    auto cond = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(2));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<ForInRange>(resultTy, inductionVar, loopCondVar, body, latch, cond, parentBlock);
}

template <>
ForInClosedRange* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ForInClosedRange* obj)
{
    auto inductionVar = GetValue<Value>(obj->base()->operands()->Get(0));
    auto loopCondVar = GetValue<Value>(obj->base()->operands()->Get(1));
    auto body = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(0));
    auto latch = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(1));
    auto cond = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(2));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<ForInClosedRange>(
        resultTy, inductionVar, loopCondVar, body, latch, cond, parentBlock);
}

template <> ForInIter* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ForInIter* obj)
{
    auto inductionVar = GetValue<Value>(obj->base()->operands()->Get(0));
    auto loopCondVar = GetValue<Value>(obj->base()->operands()->Get(1));
    auto body = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(0));

    auto latch = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(1));
    auto cond = GetValue<BlockGroup>(obj->base()->blockGroups()->Get(2));
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<ForInIter>(resultTy, inductionVar, loopCondVar, body, latch, cond, parentBlock);
}

template <> Debug* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Debug* obj)
{
    CJC_ASSERT(obj->base()->operands()->size() == 1);
    auto local = GetValue<Value>(obj->base()->operands()->Get(0));
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    auto result = builder.CreateExpression<Debug>(resultTy, local, srcCodeIdentifier, parentBlock);
    return result;
}

template <> Spawn* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Spawn* obj)
{
    auto val = GetValue<Value>(obj->base()->operands()->Get(0));
    auto func = GetValue<FuncBase>(obj->executeClosure());
    auto isClosure = obj->isExecuteClosure();
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Spawn>(resultTy, val, func, isClosure, parentBlock);
}

template <> Lambda* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Lambda* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->parentBlock());
    auto funcTy = GetType<FuncType>(obj->funcTy());
    auto isLocalFunc = obj->isLocalFunc();
    auto identifier = obj->identifier()->str();
    auto srcCodeIdentifier = obj->srcCodeIdentifier()->str();
    auto genericTypeParams = GetType<GenericType>(obj->genericTypeParams());
    auto resultTy = GetType<Type>(obj->base()->resultTy());
    return builder.CreateExpression<Lambda>(
        resultTy, funcTy, parentBlock, isLocalFunc, identifier, srcCodeIdentifier, genericTypeParams);
}

// =========================== Terminator Deserializer ==============================

template <> GoTo* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::GoTo* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto destination = GetValue<Block>(obj->base()->successors()->Get(0));
    return builder.CreateTerminator<GoTo>(destination, parentBlock);
}

template <> Branch* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Branch* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto cond = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto trueBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto falseBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto sourceExpr = CHIR::SourceExpr(obj->sourceExpr());
    auto result = builder.CreateTerminator<Branch>(cond, trueBlock, falseBlock, parentBlock);
    result->sourceExpr = sourceExpr;
    return result;
}

template <> MultiBranch* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::MultiBranch* obj)
{
    auto cond = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto successors = GetValue<Block>(obj->base()->successors());
    auto defaultBlock = successors[0];
    auto succs = std::vector<Block*>(successors.begin() + 1, successors.end());
    auto caseVals = std::vector<uint64_t>(obj->caseVals()->begin(), obj->caseVals()->end());
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    return builder.CreateTerminator<MultiBranch>(cond, defaultBlock, caseVals, succs, parentBlock);
}

template <> Exit* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::Exit* obj)
{
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    return builder.CreateTerminator<Exit>(parentBlock);
}

template <>
RaiseException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::RaiseException* obj)
{
    auto value = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto succs = GetValue<Block>(obj->base()->successors());
    if (succs.empty()) {
        return builder.CreateTerminator<RaiseException>(value, parentBlock);
    } else {
        CJC_ASSERT(succs.size() == 1);
        return builder.CreateTerminator<RaiseException>(value, succs[0], parentBlock);
    }
}

template <>
ApplyWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::ApplyWithException* obj)
{
    auto operands = GetValue<Value>(obj->base()->base()->operands());
    auto callee = operands[0];
    auto args = std::vector<Value*>(operands.begin() + 1, operands.end());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    return builder.CreateExpression<ApplyWithException>(resultTy, callee, args, sucBlock, errBlock, parentBlock);
}

template <>
InvokeWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::InvokeWithException* obj)
{
    auto operands = GetValue<Value>(obj->base()->base()->operands());
    auto object = operands[0];
    // UG need fix here methodName, methodTy
    auto args = std::vector<Value*>(operands.begin() + 1, operands.end());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // need fix
    return builder.CreateExpression<InvokeWithException>(
        resultTy, object, args, InvokeCalleeInfo{}, sucBlock, errBlock, parentBlock);
}

template <>
IntOpWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::IntOpWithException* obj)
{
    auto opKind = obj->opKind();
    auto lhs = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    if (obj->base()->base()->operands()->size() == 1) {
        return builder.CreateExpression<IntOpWithException>(
            resultTy, ExprKind(opKind), lhs, sucBlock, errBlock, parentBlock);
    }
    auto rhs = GetValue<Value>(obj->base()->base()->operands()->Get(1));
    return builder.CreateExpression<IntOpWithException>(
        resultTy, ExprKind(opKind), lhs, rhs, sucBlock, errBlock, parentBlock);
}

template <>
TypeCastWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(
    const PackageFormat::TypeCastWithException* obj)
{
    auto operand = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    return builder.CreateExpression<TypeCastWithException>(resultTy, operand, sucBlock, errBlock, parentBlock);
}

template <>
IntrinsicWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(
    const PackageFormat::IntrinsicWithException* obj)
{
    auto kind = CHIR::IntrinsicKind(obj->intrinsicKind());
    auto args = GetValue<Value>(obj->base()->base()->operands());
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());

    auto result =
        builder.CreateExpression<IntrinsicWithException>(resultTy, kind, args, sucBlock, errBlock, parentBlock);
    std::vector<Ptr<Type>> genericTypeInfo;
    auto genericArgs = GetType<Type>(obj->genericArgs());
    std::transform(genericArgs.begin(), genericArgs.end(), std::back_inserter(genericTypeInfo),
        [](Type* type) { return Ptr<Type>(type); });
    result->SetGenericTypeInfo(genericTypeInfo);
    return result;
}

template <>
AllocateWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(
    const PackageFormat::AllocateWithException* obj)
{
    auto targetType = GetType<Type>(obj->targetType());
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    return builder.CreateExpression<AllocateWithException>(resultTy, targetType, sucBlock, errBlock, parentBlock);
}

template <>
RawArrayAllocateWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(
    const PackageFormat::RawArrayAllocateWithException* obj)
{
    auto elementType = GetType<Type>(obj->elementType());
    auto size = GetValue<Value>(obj->base()->base()->operands()->Get(0));
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    return builder.CreateExpression<RawArrayAllocateWithException>(
        resultTy, elementType, size, sucBlock, errBlock, parentBlock);
}

template <>
SpawnWithException* CHIRDeserializer::CHIRDeserializerImpl::Deserialize(const PackageFormat::SpawnWithException* obj)
{
    auto operands = GetValue<Value>(obj->base()->base()->operands());
    auto val = operands[0];
    auto func = GetValue<FuncBase>(obj->executeClosure());
    auto isClosure = obj->isExecuteClosure();
    auto resultTy = GetType<Type>(obj->base()->base()->resultTy());
    // Exceptions
    auto sucBlock = GetValue<Block>(obj->base()->successors()->Get(0));
    auto errBlock = GetValue<Block>(obj->base()->successors()->Get(1));
    auto parentBlock = GetValue<Block>(obj->base()->base()->parentBlock());
    if (operands.size() > 1) {
        auto arg = operands[1];
        return builder.CreateExpression<SpawnWithException>(
            resultTy, val, arg, func, isClosure, sucBlock, errBlock, parentBlock);
    } else {
        return builder.CreateExpression<SpawnWithException>(
            resultTy, val, func, isClosure, sucBlock, errBlock, parentBlock);
    }
}

// =========================== Configuration ==============================

void CHIRDeserializer::CHIRDeserializerImpl::ConfigBase(const PackageFormat::Base* expr, Base& obj)
{
    Base base;
    auto annos = expr->annos();
    auto annoTypes = expr->annos_type();
    for (unsigned i = 0; i < annos->size(); ++i) {
        auto anno = annos->Get(i);
        switch (annoTypes->Get(i)) {
            case PackageFormat::Annotation::Annotation_needCheckArrayBound:
                base.Set<NeedCheckArrayBound>(static_cast<const PackageFormat::NeedCheckArrayBound*>(anno)->need());
                break;
            case PackageFormat::Annotation::Annotation_needCheckCast:
                base.Set<NeedCheckCast>(static_cast<const PackageFormat::NeedCheckCast*>(anno)->need());
                break;
            case PackageFormat::Annotation::Annotation_debugLocationInfo:
                base.SetDebugLocation(Create<DebugLocation>(static_cast<const PackageFormat::DebugLocation*>(anno)));
                break;
            case PackageFormat::Annotation::Annotation_debugLocationInfoForWarning:
                base.Set<DebugLocationInfoForWarning>(
                    Create<DebugLocation>(static_cast<const PackageFormat::DebugLocation*>(anno)));
                break;
            case PackageFormat::Annotation::Annotation_linkTypeInfo:
                base.Set<CHIR::LinkTypeInfo>(
                    Cangjie::Linkage(static_cast<const PackageFormat::LinkTypeInfo*>(anno)->linkage()));
                break;
            case PackageFormat::Annotation::Annotation_skipCheck:
                base.Set<CHIR::SkipCheck>(
                    CHIR::SkipKind(static_cast<const PackageFormat::SkipCheck*>(anno)->skipKind()));
                break;
            case PackageFormat::Annotation::Annotation_neverOverflowInfo:
                base.Set<CHIR::NeverOverflowInfo>(
                    static_cast<const PackageFormat::NeverOverflowInfo*>(anno)->neverOverflow());
                break;
            default:
                continue;
        }
    }
    obj.CopyAnnotationMapFrom(base);
}

void CHIRDeserializer::CHIRDeserializerImpl::ConfigValue(const PackageFormat::Value* expr, Value& obj)
{
    CJC_NULLPTR_CHECK(expr);
    ConfigBase(expr->base(), obj);
    obj.AppendAttributeInfo(CreateAttr(expr->attributes()));
}

void CHIRDeserializer::CHIRDeserializerImpl::ConfigCustomTypeDef(
    const PackageFormat::CustomTypeDef* def, CustomTypeDef& obj)
{
    ConfigBase(def->base(), obj);
    // UG need fix here
    auto declaredMethods = GetValue<FuncBase>(def->methods());
    for (auto declaredMethod : declaredMethods) {
        obj.AddMethod(declaredMethod);
    }
    // UG need fix here

    auto staticMemberVars = GetValue<GlobalVarBase>(def->staticMemberVars());
    for (auto var : staticMemberVars) {
        obj.AddStaticMemberVar(var);
    }

    auto instanceMemberVars = Create<MemberVarInfo>(def->instanceMemberVars());
    for (auto var : instanceMemberVars) {
        obj.AddInstanceVar(var);
    }
    // UG need fix here
    obj.SetAnnoInfo(Create<AnnoInfo>(def->annoInfo()));
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::Lambda* buffer, Lambda& obj)
{
    ConfigBase(buffer->base()->base(), obj);
    // the parameter will be inserted into Lambda when Parameter is created.
    GetValue<Parameter>(buffer->params());
    obj.SetCapturedVars(GetValue<Value>(buffer->capturedVars()));
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::Func* buffer, Func& obj)
{
    ConfigBase(buffer->base()->base(), obj);
    obj.AppendAttributeInfo(CreateAttr(buffer->base()->attributes()));
    obj.InitBody(*GetValue<BlockGroup>(buffer->body()));
    obj.SetFuncKind(FuncKind(buffer->funcKind()));
    // the parameter will be inserted into Func when Parameter is created.
    GetValue<Parameter>(buffer->params());

    obj.SetRawMangledName(buffer->rawMangledName()->str());
    // UG need fix here genericDecl
    obj.SetAnnoInfo(Create<AnnoInfo>(buffer->annoInfo()));
    obj.SetPropLocation(Create<DebugLocation>(buffer->propLoc()));
    obj.SetParentRawMangledName(buffer->parentName()->str());
    obj.SetFastNative(buffer->isFastNative());
    obj.SetCFFIWrapper(buffer->isCFFIWrapper());
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::LocalVar* buffer, LocalVar& obj)
{
    ConfigValue(buffer->base(), obj);

    // set identifier for convenient comparision.
    obj.identifier = std::string("%") + buffer->base()->identifier()->str();
    if (buffer->isRetVal()) {
        obj.SetRetValue();
    }
}

template <>
void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::BlockGroup* buffer, BlockGroup& obj)
{
    ConfigBase(buffer->base()->base(), obj);
    GetValue<Block>(buffer->blocks());
    obj.SetEntryBlock(GetValue<Block>(buffer->entryBlock()));
    obj.identifier = buffer->base()->identifier()->str();
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::Block* buffer, Block& obj)
{
    ConfigBase(buffer->base()->base(), obj);
    obj.AppendAttributeInfo(CreateAttr(buffer->base()->attributes()));
    if (buffer->isLandingPadBlock()) {
        obj.SetExceptions(GetType<ClassType>(buffer->exceptionCatchList()));
    }
    obj.AppendExpressions(GetExpression<Expression>(buffer->exprs()));
    obj.identifier = std::string("#") + buffer->base()->identifier()->str();
    obj.predecessors.clear();
    obj.predecessors = GetValue<Block>(buffer->predecessors());
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::GlobalVar* buffer, GlobalVar& obj)
{
    ConfigBase(buffer->base()->base(), obj);
    obj.SetAnnoInfo(Create<AnnoInfo>(buffer->annoInfo()));
    if (auto initializer = DynamicCast<LiteralValue*>(GetValue<Value>(buffer->defaultInitVal()))) {
        obj.SetInitializer(*initializer);
    } else if (auto initFunc = GetValue<Func>(buffer->associatedInitFunc())) {
        obj.SetInitFunc(*initFunc);
    }
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::EnumDef* buffer, EnumDef& obj)
{
    ConfigCustomTypeDef(buffer->base(), obj);
    for (auto info : Create<EnumCtorInfo>(buffer->ctors())) {
        obj.AddCtor(info);
    }
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::StructDef* buffer, StructDef& obj)
{
    ConfigCustomTypeDef(buffer->base(), obj);
    obj.SetCStruct(buffer->isCStruct());
}

template <> void CHIRDeserializer::CHIRDeserializerImpl::Config(const PackageFormat::ClassDef* buffer, ClassDef& obj)
{
    // UG need fix here
    ConfigCustomTypeDef(buffer->base(), obj);
    // UG need fix here
    for (auto info : Create<AbstractMethodInfo>(buffer->abstractMethods())) {
        obj.AddAbstractMethod(info);
    }
}
// =========================== Fetch by ID ==============================

Value* CHIRDeserializer::CHIRDeserializerImpl::GetValue(uint32_t id)
{
    if (id2Value.count(id) == 0) {
        switch (PackageFormat::ValueElem(pool->values_type()->Get(id - 1))) {
            case PackageFormat::ValueElem_BoolLiteral:
                id2Value[id] = Deserialize<BoolLiteral>(
                    static_cast<const PackageFormat::BoolLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::BoolLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_RuneLiteral:
                id2Value[id] = Deserialize<RuneLiteral>(
                    static_cast<const PackageFormat::RuneLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::RuneLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_StringLiteral:
                id2Value[id] = Deserialize<StringLiteral>(
                    static_cast<const PackageFormat::StringLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(
                    static_cast<const PackageFormat::StringLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_IntLiteral:
                id2Value[id] =
                    Deserialize<IntLiteral>(static_cast<const PackageFormat::IntLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::IntLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_FloatLiteral:
                id2Value[id] = Deserialize<FloatLiteral>(
                    static_cast<const PackageFormat::FloatLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(
                    static_cast<const PackageFormat::FloatLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_UnitLiteral:
                id2Value[id] = Deserialize<UnitLiteral>(
                    static_cast<const PackageFormat::UnitLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::UnitLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_NullLiteral:
                id2Value[id] = Deserialize<NullLiteral>(
                    static_cast<const PackageFormat::NullLiteral*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::NullLiteral*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_Parameter:
                id2Value[id] =
                    Deserialize<Parameter>(static_cast<const PackageFormat::Parameter*>(pool->values()->Get(id - 1)));
                ConfigValue(
                    static_cast<const PackageFormat::Parameter*>(pool->values()->Get(id - 1))->base(), *id2Value[id]);
                break;
            case PackageFormat::ValueElem_LocalVar:
                id2Value[id] =
                    Deserialize<LocalVar>(static_cast<const PackageFormat::LocalVar*>(pool->values()->Get(id - 1)));
                Config(static_cast<const PackageFormat::LocalVar*>(pool->values()->Get(id - 1)),
                    *StaticCast<LocalVar*>(id2Value[id]));
                break;
            case PackageFormat::ValueElem_GlobalVar:
                id2Value[id] =
                    Deserialize<GlobalVar>(static_cast<const PackageFormat::GlobalVar*>(pool->values()->Get(id - 1)));
                break;
            case PackageFormat::ValueElem_Func:
                id2Value[id] = Deserialize<Func>(static_cast<const PackageFormat::Func*>(pool->values()->Get(id - 1)));
                break;
            case PackageFormat::ValueElem_ImportedVar:
                id2Value[id] = Deserialize<ImportedVar>(
                    static_cast<const PackageFormat::ImportedVar*>(pool->values()->Get(id - 1)));
                ConfigValue(static_cast<const PackageFormat::ImportedVar*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_ImportedFunc:
                id2Value[id] = Deserialize<ImportedFunc>(
                    static_cast<const PackageFormat::ImportedFunc*>(pool->values()->Get(id - 1)));
                ConfigValue(
                    static_cast<const PackageFormat::ImportedFunc*>(pool->values()->Get(id - 1))->base()->base(),
                    *id2Value[id]);
                break;
            case PackageFormat::ValueElem_Block:
                id2Value[id] =
                    Deserialize<Block>(static_cast<const PackageFormat::Block*>(pool->values()->Get(id - 1)));
                break;
            case PackageFormat::ValueElem_BlockGroup:
                id2Value[id] =
                    Deserialize<BlockGroup>(static_cast<const PackageFormat::BlockGroup*>(pool->values()->Get(id - 1)));
                break;
            case PackageFormat::ValueElem_NONE:
                InternalError("Unsupported value type.");
                break;
        }
    }
    return id2Value[id];
}

template <typename T> T* CHIRDeserializer::CHIRDeserializerImpl::GetValue(uint32_t id)
{
    if (id == 0) {
        return nullptr;
    }
    return DynamicCast<T*>(GetValue(id));
}

Type* CHIRDeserializer::CHIRDeserializerImpl::GetType(uint32_t id)
{
    if (id2Type.count(id) == 0) {
        switch (PackageFormat::TypeElem(pool->types_type()->Get(id - 1))) {
            case PackageFormat::TypeElem_RuneType:
                id2Type[id] =
                    Deserialize<RuneType>(static_cast<const PackageFormat::RuneType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_BooleanType:
                id2Type[id] = Deserialize<BooleanType>(
                    static_cast<const PackageFormat::BooleanType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_UnitType:
                id2Type[id] =
                    Deserialize<UnitType>(static_cast<const PackageFormat::UnitType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_NothingType:
                id2Type[id] = Deserialize<NothingType>(
                    static_cast<const PackageFormat::NothingType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_IntType:
                id2Type[id] =
                    Deserialize<IntType>(static_cast<const PackageFormat::IntType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_FloatType:
                id2Type[id] =
                    Deserialize<FloatType>(static_cast<const PackageFormat::FloatType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_TupleType:
                id2Type[id] =
                    Deserialize<TupleType>(static_cast<const PackageFormat::TupleType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_ClosureType:
                id2Type[id] = Deserialize<ClosureType>(
                    static_cast<const PackageFormat::ClosureType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_RawArrayType:
                id2Type[id] = Deserialize<RawArrayType>(
                    static_cast<const PackageFormat::RawArrayType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_VArrayType:
                id2Type[id] =
                    Deserialize<VArrayType>(static_cast<const PackageFormat::VArrayType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_FuncType:
                id2Type[id] =
                    Deserialize<FuncType>(static_cast<const PackageFormat::FuncType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_EnumType:
                id2Type[id] =
                    Deserialize<EnumType>(static_cast<const PackageFormat::EnumType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_StructType:
                id2Type[id] =
                    Deserialize<StructType>(static_cast<const PackageFormat::StructType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_ClassType:
                id2Type[id] =
                    Deserialize<ClassType>(static_cast<const PackageFormat::ClassType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_CStringType:
                id2Type[id] = Deserialize<CStringType>(
                    static_cast<const PackageFormat::CStringType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_CPointerType:
                id2Type[id] = Deserialize<CPointerType>(
                    static_cast<const PackageFormat::CPointerType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_GenericType:
                id2Type[id] = Deserialize<GenericType>(
                    static_cast<const PackageFormat::GenericType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_RefType:
                id2Type[id] =
                    Deserialize<RefType>(static_cast<const PackageFormat::RefType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_BoxType:
                // enable this will crash the compilation of LSP, need to fix
                //     id2Type[id] =
                //         Deserialize<BoxType>(static_cast<const PackageFormat::BoxType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_VoidType:
                id2Type[id] =
                    Deserialize<VoidType>(static_cast<const PackageFormat::VoidType*>(pool->types()->Get(id - 1)));
                break;
            case PackageFormat::TypeElem_NONE:
                id2Type[id] = nullptr;
                break;
        }
    }
    return id2Type[id];
}

template <typename T> T* CHIRDeserializer::CHIRDeserializerImpl::GetType(uint32_t id)
{
    if (id == 0) {
        return nullptr;
    }
    return StaticCast<T*>(GetType(id));
}

Expression* CHIRDeserializer::CHIRDeserializerImpl::GetExpression(uint32_t id)
{
    if (id2Expression.count(id) == 0) {
        switch (PackageFormat::ExpressionElem(pool->exprs_type()->Get(id - 1))) {
            case PackageFormat::ExpressionElem_UnaryExpression:
                id2Expression[id] = Deserialize<UnaryExpression>(
                    static_cast<const PackageFormat::UnaryExpression*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::UnaryExpression*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_BinaryExpression:
                id2Expression[id] = Deserialize<BinaryExpression>(
                    static_cast<const PackageFormat::BinaryExpression*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::BinaryExpression*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Constant:
                id2Expression[id] =
                    Deserialize<Constant>(static_cast<const PackageFormat::Constant*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Constant*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Allocate:
                id2Expression[id] =
                    Deserialize<Allocate>(static_cast<const PackageFormat::Allocate*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Allocate*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Load:
                id2Expression[id] = Deserialize<Cangjie::CHIR::Load>(
                    static_cast<const PackageFormat::Load*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Load*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Store:
                id2Expression[id] =
                    Deserialize<Store>(static_cast<const PackageFormat::Store*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Store*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_GetElementRef:
                id2Expression[id] = Deserialize<GetElementRef>(
                    static_cast<const PackageFormat::GetElementRef*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::GetElementRef*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_StoreElementRef:
                id2Expression[id] = Deserialize<StoreElementRef>(
                    static_cast<const PackageFormat::StoreElementRef*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::StoreElementRef*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Apply:
                id2Expression[id] =
                    Deserialize<Apply>(static_cast<const PackageFormat::Apply*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Apply*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Invoke:
                id2Expression[id] =
                    Deserialize<Invoke>(static_cast<const PackageFormat::Invoke*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Invoke*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_TypeCast:
                id2Expression[id] =
                    Deserialize<TypeCast>(static_cast<const PackageFormat::TypeCast*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::TypeCast*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_InstanceOf:
                id2Expression[id] =
                    Deserialize<InstanceOf>(static_cast<const PackageFormat::InstanceOf*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::InstanceOf*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Box:
                id2Expression[id] =
                    Deserialize<Box>(static_cast<const PackageFormat::Box*>(pool->exprs()->Get(id - 1)));
                break;
            case PackageFormat::ExpressionElem_UnBox:
                id2Expression[id] =
                    Deserialize<UnBox>(static_cast<const PackageFormat::UnBox*>(pool->exprs()->Get(id - 1)));
                break;
            case PackageFormat::ExpressionElem_GoTo:
                id2Expression[id] =
                    Deserialize<GoTo>(static_cast<const PackageFormat::GoTo*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::GoTo*>(pool->exprs()->Get(id - 1))->base()->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Branch:
                id2Expression[id] =
                    Deserialize<Branch>(static_cast<const PackageFormat::Branch*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::Branch*>(pool->exprs()->Get(id - 1))->base()->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_MultiBranch:
                id2Expression[id] = Deserialize<MultiBranch>(
                    static_cast<const PackageFormat::MultiBranch*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::MultiBranch*>(pool->exprs()->Get(id - 1))->base()->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Exit:
                id2Expression[id] =
                    Deserialize<Exit>(static_cast<const PackageFormat::Exit*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Exit*>(pool->exprs()->Get(id - 1))->base()->base()->base(),
                    *static_cast<Base*>(id2Expression[id]));
                break;
            case PackageFormat::ExpressionElem_RaiseException:
                id2Expression[id] = Deserialize<RaiseException>(
                    static_cast<const PackageFormat::RaiseException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::RaiseException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_ApplyWithException:
                id2Expression[id] = Deserialize<ApplyWithException>(
                    static_cast<const PackageFormat::ApplyWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::ApplyWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_InvokeWithException:
                id2Expression[id] = Deserialize<InvokeWithException>(
                    static_cast<const PackageFormat::InvokeWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::InvokeWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_InvokeStatic:
                id2Expression[id] = Deserialize<InvokeStatic>(
                    static_cast<const PackageFormat::InvokeStatic*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::InvokeStatic*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_InvokeStaticWithException:
                id2Expression[id] = Deserialize<InvokeStaticWithException>(
                    static_cast<const PackageFormat::InvokeStaticWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::InvokeStaticWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_IntOpWithException:
                id2Expression[id] = Deserialize<IntOpWithException>(
                    static_cast<const PackageFormat::IntOpWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::IntOpWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_TypeCastWithException:
                id2Expression[id] = Deserialize<TypeCastWithException>(
                    static_cast<const PackageFormat::TypeCastWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::TypeCastWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_IntrinsicWithException:
                id2Expression[id] = Deserialize<IntrinsicWithException>(
                    static_cast<const PackageFormat::IntrinsicWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::IntrinsicWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_AllocateWithException:
                id2Expression[id] = Deserialize<AllocateWithException>(
                    static_cast<const PackageFormat::AllocateWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::AllocateWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_RawArrayAllocateWithException:
                id2Expression[id] = Deserialize<RawArrayAllocateWithException>(
                    static_cast<const PackageFormat::RawArrayAllocateWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::RawArrayAllocateWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_SpawnWithException:
                id2Expression[id] = Deserialize<SpawnWithException>(
                    static_cast<const PackageFormat::SpawnWithException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::SpawnWithException*>(pool->exprs()->Get(id - 1))
                               ->base()
                               ->base()
                               ->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Tuple:
                id2Expression[id] =
                    Deserialize<Tuple>(static_cast<const PackageFormat::Tuple*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Tuple*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Field:
                id2Expression[id] =
                    Deserialize<Field>(static_cast<const PackageFormat::Field*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Field*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_RawArrayAllocate:
                id2Expression[id] = Deserialize<RawArrayAllocate>(
                    static_cast<const PackageFormat::RawArrayAllocate*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::RawArrayAllocate*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_RawArrayLiteralInit:
                id2Expression[id] = Deserialize<RawArrayLiteralInit>(
                    static_cast<const PackageFormat::RawArrayLiteralInit*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::RawArrayLiteralInit*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_RawArrayInitByValue:
                id2Expression[id] = Deserialize<RawArrayInitByValue>(
                    static_cast<const PackageFormat::RawArrayInitByValue*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::RawArrayInitByValue*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_VArray:
                id2Expression[id] =
                    Deserialize<VArray>(static_cast<const PackageFormat::VArray*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::VArray*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_VArrayBd:
                id2Expression[id] =
                    Deserialize<VArrayBuilder>(static_cast<const PackageFormat::VArrayBd*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::VArrayBd*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_GetException:
                id2Expression[id] = Deserialize<GetException>(
                    static_cast<const PackageFormat::GetException*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::GetException*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Intrinsic:
                id2Expression[id] =
                    Deserialize<Intrinsic>(static_cast<const PackageFormat::Intrinsic*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Intrinsic*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_If:
                id2Expression[id] = Deserialize<If>(static_cast<const PackageFormat::If*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::If*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Loop:
                id2Expression[id] =
                    Deserialize<Loop>(static_cast<const PackageFormat::Loop*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Loop*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_ForInRange:
                id2Expression[id] =
                    Deserialize<ForInRange>(static_cast<const PackageFormat::ForInRange*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::ForInRange*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_ForInIter:
                id2Expression[id] =
                    Deserialize<ForInIter>(static_cast<const PackageFormat::ForInIter*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::ForInIter*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_ForInClosedRange:
                id2Expression[id] = Deserialize<ForInClosedRange>(
                    static_cast<const PackageFormat::ForInClosedRange*>(pool->exprs()->Get(id - 1)));
                ConfigBase(
                    static_cast<const PackageFormat::ForInClosedRange*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Debug:
                id2Expression[id] =
                    Deserialize<Debug>(static_cast<const PackageFormat::Debug*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Debug*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Spawn:
                id2Expression[id] =
                    Deserialize<Spawn>(static_cast<const PackageFormat::Spawn*>(pool->exprs()->Get(id - 1)));
                ConfigBase(static_cast<const PackageFormat::Spawn*>(pool->exprs()->Get(id - 1))->base()->base(),
                    *id2Expression[id]);
                break;
            case PackageFormat::ExpressionElem_Lambda:
                id2Expression[id] =
                    Deserialize<Lambda>(static_cast<const PackageFormat::Lambda*>(pool->exprs()->Get(id - 1)));
                Config(
                    static_cast<const PackageFormat::Lambda*>(pool->exprs()->Get(id - 1)), *GetExpression<Lambda>(id));
                break;
            case PackageFormat::ExpressionElem_NONE:
                InternalError("unsupported expression type in chir deserialization.");
                break;
        }
    }
    return id2Expression[id];
}

template <typename T> T* CHIRDeserializer::CHIRDeserializerImpl::GetExpression(uint32_t id)
{
    if (id == 0) {
        return nullptr;
    }
    return StaticCast<T*>(GetExpression(id));
}

CustomTypeDef* CHIRDeserializer::CHIRDeserializerImpl::GetCustomTypeDef(uint32_t id)
{
    if (id2CustomTypeDef.count(id) == 0) {
        switch (PackageFormat::CustomTypeDefElem(pool->defs_type()->Get(id - 1))) {
            case PackageFormat::CustomTypeDefElem_EnumDef:
                id2CustomTypeDef[id] =
                    Deserialize<EnumDef>(static_cast<const PackageFormat::EnumDef*>(pool->defs()->Get(id - 1)));
                break;
            case PackageFormat::CustomTypeDefElem_StructDef:
                id2CustomTypeDef[id] =
                    Deserialize<StructDef>(static_cast<const PackageFormat::StructDef*>(pool->defs()->Get(id - 1)));
                break;
            case PackageFormat::CustomTypeDefElem_ClassDef:
                id2CustomTypeDef[id] =
                    Deserialize<ClassDef>(static_cast<const PackageFormat::ClassDef*>(pool->defs()->Get(id - 1)));
                break;
            case PackageFormat::CustomTypeDefElem_NONE:
                id2CustomTypeDef[id] = nullptr;
                break;
        }
    }
    return id2CustomTypeDef[id];
}

template <typename T> T* CHIRDeserializer::CHIRDeserializerImpl::GetCustomTypeDef(uint32_t id)
{
    if (id == 0) {
        return nullptr;
    }
    return StaticCast<T*>(GetCustomTypeDef(id));
}

// =========================== Utilities ==============================

void* CHIRDeserializer::CHIRDeserializerImpl::Read(const std::string& fileName) const
{
    std::ifstream infile;
    infile.open(fileName, std::ios::binary | std::ios::in);
    if (!infile.is_open()) {
        abort();
    }
    infile.seekg(0, std::ios::end);
    std::streampos pos = infile.tellg();
    std::streamoff length = static_cast<std::streamoff>(pos);
    constexpr std::streamoff LENGTH_LIMIT = 4LL * 1024 * 1024 * 1024;
    if (length >= LENGTH_LIMIT) {
        Errorln(fileName, " length exceed the max file length: 4 GB");
        infile.close();
        return nullptr;
    }
    infile.seekg(0, std::ios::beg);
    void* bufferPointer = new char[static_cast<unsigned long>(length)];
    infile.read(static_cast<char*>(bufferPointer), length);
    infile.close();
    return bufferPointer;
}

// =========================== Entry ==================================
void CHIRDeserializer::CHIRDeserializerImpl::Run(const PackageFormat::CHIRPackage* package)
{
    pool = package;
    builder.CreatePackage(pool->name()->str());

    // To keep order, get CustomTypeDef first
    for (unsigned id = 1; id <= pool->defs()->size(); ++id) {
        GetCustomTypeDef<CustomTypeDef>(id);
    }

    // deserialize values
    for (unsigned id = 1; id <= pool->values()->size(); ++id) {
        // lambda's parameter may appear earlier than the lambda self.
        if (pool->values_type()->Get(id - 1) ==
                static_cast<std::underlying_type_t<PackageFormat::ValueElem>>(PackageFormat::ValueElem_Parameter) &&
            static_cast<const PackageFormat::Parameter*>(pool->values()->Get(id - 1))->ownedLambda() != 0) {
            continue;
        }
        GetValue<Value>(id);
    }

    for (unsigned id = 1; id <= pool->values()->size(); ++id) {
        // lazy config Func to keep order
        switch (pool->values_type()->Get(id - 1)) {
            case PackageFormat::ValueElem_Block:
                Config(static_cast<const PackageFormat::Block*>(pool->values()->Get(id - 1)), *GetValue<Block>(id));
                break;
            case PackageFormat::ValueElem_BlockGroup:
                Config(static_cast<const PackageFormat::BlockGroup*>(pool->values()->Get(id - 1)),
                    *GetValue<BlockGroup>(id));
                break;
            case PackageFormat::ValueElem_GlobalVar:
                Config(static_cast<const PackageFormat::GlobalVar*>(pool->values()->Get(id - 1)),
                    *GetValue<GlobalVar>(id));
                break;
            case PackageFormat::ValueElem_Func:
                Config(static_cast<const PackageFormat::Func*>(pool->values()->Get(id - 1)), *GetValue<Func>(id));
                break;
            default:
                // do nothing
                break;
        }
    }

    // Config CustomTypeDef
    for (unsigned id = 1; id <= pool->defs()->size(); ++id) {
        switch (pool->defs_type()->Get(id - 1)) {
            case PackageFormat::CustomTypeDefElem_EnumDef:
                Config(static_cast<const PackageFormat::EnumDef*>(pool->defs()->Get(id - 1)),
                    *GetCustomTypeDef<EnumDef>(id));
                break;
            case PackageFormat::CustomTypeDefElem_StructDef:
                Config(static_cast<const PackageFormat::StructDef*>(pool->defs()->Get(id - 1)),
                    *GetCustomTypeDef<StructDef>(id));
                break;
            case PackageFormat::CustomTypeDefElem_ClassDef:
                Config(static_cast<const PackageFormat::ClassDef*>(pool->defs()->Get(id - 1)),
                    *GetCustomTypeDef<ClassDef>(id));
                break;
            default:
                break;
        }
    }

    // Config generic types
    for (auto entry : genericTypeConfig) {
        // Fill in sub-fields
        auto upperBounds = GetType<Type>(entry.second->upperBounds());
        entry.first->SetUpperBounds(upperBounds);
    }

    // Package self's member
    builder.GetCurPackage()->SetPackageInitFunc(GetValue<Func>(pool->globalInitFunc()));
}
