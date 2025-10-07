// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "cangjie/CHIR/Serializer/CHIRSerializer.h"
#include "CHIRSerializerImpl.h"
#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/GeneratedFromForIn.h"
#include "cangjie/CHIR/IntrinsicKind.h"
#include "cangjie/CHIR/ToStringUtils.h"
#include "cangjie/CHIR/Type/ClassDef.h"
#include "cangjie/CHIR/Type/EnumDef.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/Type/StructDef.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Utils/ICEUtil.h"
#include "flatbuffers/PackageFormat_generated.h"
#include "flatbuffers/buffer.h"
#include "cangjie/CHIR/UserDefinedType.h"

#include <algorithm>

using namespace Cangjie::CHIR;

void CHIRSerializer::Serialize(const Package& package, const std::string filename)
{
    CHIRSerializerImpl serializer(package);
    serializer.Initialize();
    serializer.Dispatch();
    serializer.Save(filename);
}

// ========================== ID Fetchers ==============================

template <typename T, typename E> std::vector<uint32_t> CHIRSerializer::CHIRSerializerImpl::GetId(std::vector<E*> vec)
{
    std::vector<uint32_t> indices;
    for (E* elem : vec) {
        indices.emplace_back(GetId<T>(static_cast<const T*>(elem)));
    }
    return indices;
}

template <typename T, typename E>
std::vector<uint32_t> CHIRSerializer::CHIRSerializerImpl::GetId(std::vector<Ptr<E>> vec)
{
    std::vector<uint32_t> indices;
    for (Ptr<E> elem : vec) {
        indices.emplace_back(GetId<T>(static_cast<const T*>(elem.get())));
    }
    return indices;
}

template <> uint32_t CHIRSerializer::CHIRSerializerImpl::GetId(const Value* obj)
{
    if (value2Id.count(obj) == 0) {
        value2Id[obj] = ++valueCount;
        allValue.emplace_back(0);
        valueKind.emplace_back(0);
        valueQueue.push_back(obj);
    }
    return value2Id[obj];
}

template <> uint32_t CHIRSerializer::CHIRSerializerImpl::GetId(const Type* obj)
{
    if (type2Id.count(obj) == 0) {
        type2Id[obj] = ++typeCount;
        allType.emplace_back(0);
        typeKind.emplace_back(0);
        typeQueue.push(obj);
    }
    return type2Id[obj];
}

template <> uint32_t CHIRSerializer::CHIRSerializerImpl::GetId(const Expression* obj)
{
    if (expr2Id.count(obj) == 0) {
        expr2Id[obj] = ++exprCount;
        allExpression.emplace_back(0);
        exprKind.emplace_back(0);
        exprQueue.push(obj);
    }
    return expr2Id[obj];
}

template <> uint32_t CHIRSerializer::CHIRSerializerImpl::GetId(const CustomTypeDef* obj)
{
    if (def2Id.count(obj) == 0) {
        def2Id[obj] = ++defCount;
        allCustomTypeDef.emplace_back(0);
        defKind.emplace_back(0);
        defQueue.push_back(obj);
    }
    return def2Id[obj];
}

// ========================== Helper Serializers ===============================

template <typename FBT, typename T>
std::vector<flatbuffers::Offset<FBT>> CHIRSerializer::CHIRSerializerImpl::SerializeVec(const std::vector<T>& vec)
{
    std::vector<flatbuffers::Offset<FBT>> retval;
    for (T elem : vec) {
        retval.emplace_back(Serialize<FBT>(elem));
    }
    return retval;
}

template <>
flatbuffers::Offset<PackageFormat::DebugLocation> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const DebugLocation& obj)
{
    auto beginPos = PackageFormat::CreatePos(builder, obj.GetBeginPos().line, obj.GetBeginPos().column);
    auto endPos = PackageFormat::CreatePos(builder, obj.GetEndPos().line, obj.GetEndPos().column);
    auto scope = obj.GetScopeInfo();
    return PackageFormat::CreateDebugLocationDirect(
        builder, obj.GetAbsPath().c_str(), obj.GetFileID(), beginPos, endPos, &scope);
}

template <>
flatbuffers::Offset<PackageFormat::AnnoInfo> CHIRSerializer::CHIRSerializerImpl::Serialize(const AnnoInfo& obj)
{
    return PackageFormat::CreateAnnoInfoDirect(builder, obj.mangledName.data());
}

static void Empty(Annotation*) {}

template <> flatbuffers::Offset<PackageFormat::Base> CHIRSerializer::CHIRSerializerImpl::Serialize(const Base& obj)
{
    auto annoTypes = std::vector<uint8_t>();
    auto annos = std::vector<flatbuffers::Offset<void>>();
    std::unordered_map<std::type_index, std::function<void(Annotation*)>> annoHandler;

    // NeedCheckArrayBound
    annoHandler[typeid(CHIR::NeedCheckArrayBound)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_needCheckArrayBound);
        annos.emplace_back(PackageFormat::CreateNeedCheckArrayBound(
            builder, NeedCheckArrayBound::Extract(StaticCast<NeedCheckArrayBound*>(anno)))
                               .Union());
    };

    // NeedCheckCast
    annoHandler[typeid(CHIR::NeedCheckCast)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_needCheckCast);
        annos.emplace_back(
            PackageFormat::CreateNeedCheckCast(builder, NeedCheckCast::Extract(StaticCast<NeedCheckCast*>(anno)))
                .Union());
    };

    // DebugLocationInfo
    annoTypes.emplace_back(PackageFormat::Annotation::Annotation_debugLocationInfo);
    annos.emplace_back(Serialize<PackageFormat::DebugLocation>(obj.GetDebugLocation()).Union());

    // DebugLocationInfoForWarning
    annoHandler[typeid(CHIR::DebugLocationInfoForWarning)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_debugLocationInfoForWarning);
        annos.emplace_back(Serialize<PackageFormat::DebugLocation>(
            DebugLocationInfoForWarning::Extract(StaticCast<DebugLocationInfoForWarning*>(anno)))
                               .Union());
    };

    // LinkTypeInfo
    annoHandler[typeid(CHIR::LinkTypeInfo)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_linkTypeInfo);
        annos.emplace_back(PackageFormat::CreateLinkTypeInfo(
            builder, PackageFormat::Linkage(LinkTypeInfo::Extract(StaticCast<CHIR::LinkTypeInfo*>(anno))))
                               .Union());
    };

    // SkipCheck
    annoHandler[typeid(CHIR::SkipCheck)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_skipCheck);
        annos.emplace_back(PackageFormat::CreateSkipCheck(
            builder, PackageFormat::SkipKind(SkipCheck::Extract(StaticCast<CHIR::SkipCheck*>(anno))))
                               .Union());
    };

    // NeverOverflowInfo
    annoHandler[typeid(CHIR::NeverOverflowInfo)] = [this, &annos, &annoTypes](Annotation* anno) {
        annoTypes.push_back(PackageFormat::Annotation::Annotation_neverOverflowInfo);
        annos.emplace_back(PackageFormat::CreateNeverOverflowInfo(
            builder, NeverOverflowInfo::Extract(StaticCast<CHIR::NeverOverflowInfo*>(anno)))
                               .Union());
    };

    annoHandler[typeid(CHIR::GeneratedFromForIn)] = Empty;

    for (auto& entry : obj.GetAnno().GetAnnos()) {
        if (annoHandler.count(entry.first) != 0) {
            annoHandler.at(entry.first)(entry.second.get());
        } else {
            InternalError(std::string("unsupported chir annotation: ") + entry.first.name());
        }
    }
    return PackageFormat::CreateBaseDirect(builder, &annoTypes, &annos);
}

template <>
flatbuffers::Offset<PackageFormat::MemberVarInfo> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const MemberVarInfo& obj)
{
    auto name = obj.name;
    auto rawMangledName = obj.rawMangledName;
    auto type = GetId<Type>(obj.type);
    auto attributes = obj.attributeInfo.GetRawAttrs().to_ulong();
    auto loc = Serialize<PackageFormat::DebugLocation>(obj.loc);
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.annoInfo);
    return PackageFormat::CreateMemberVarInfoDirect(
        builder, name.data(), rawMangledName.data(), type, attributes, loc, annoInfo);
}

template <>
flatbuffers::Offset<PackageFormat::EnumCtorInfo> CHIRSerializer::CHIRSerializerImpl::Serialize(const EnumCtorInfo& obj)
{
    return PackageFormat::CreateEnumCtorInfoDirect(
        builder, obj.name.data(), obj.mangledName.data(), GetId<Type>(obj.funcType));
}

template <>
flatbuffers::Offset<PackageFormat::VTableElement> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const std::pair<std::string, std::pair<const FuncType*, FuncBase*>>& obj)
{
    auto virtualMethodName = obj.first;
    auto virtualMethodType = GetId<Type>(obj.second.first);
    auto dispatchedFunc = GetId<Value>(obj.second.second);
    return PackageFormat::CreateVTableElementDirect(
        builder, virtualMethodName.data(), virtualMethodType, dispatchedFunc);
}

template <>
flatbuffers::Offset<PackageFormat::AbstractMethodParam> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const AbstractMethodParam& obj)
{
    auto paramName = obj.paramName;
    auto paramType = GetId<Type>(obj.type);
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.annoInfo);
    return PackageFormat::CreateAbstractMethodParamDirect(builder, paramName.data(), paramType, annoInfo);
}

template <>
flatbuffers::Offset<PackageFormat::AbstractMethodInfo> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const AbstractMethodInfo& obj)
{
    auto methodName = obj.methodName;
    auto mangledName = obj.GetASTMangledName();
    auto methodType = GetId<Type>(obj.methodTy);
    auto paramsInfo = SerializeVec<PackageFormat::AbstractMethodParam>(obj.paramInfos);
    auto attributes = obj.attributeInfo.GetRawAttrs().to_ulong();
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.annoInfo);
    return PackageFormat::CreateAbstractMethodInfoDirect(
        builder, methodName.data(), mangledName.data(), methodType, &paramsInfo, attributes, annoInfo);
}

// ========================== Type Serializers =================================

template <> flatbuffers::Offset<PackageFormat::Type> CHIRSerializer::CHIRSerializerImpl::Serialize(const Type& obj)
{
    auto typeId = GetId<Type>(&obj);
    auto kind = PackageFormat::CHIRTypeKind(obj.GetTypeKind());
    auto argTys = GetId<Type>(obj.GetTypeArgs());
    auto refDims = obj.GetRefDims();
    return PackageFormat::CreateTypeDirect(builder, kind, typeId, &argTys, refDims);
}

template <>
flatbuffers::Offset<PackageFormat::RuneType> CHIRSerializer::CHIRSerializerImpl::Serialize(const RuneType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateRuneType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::BooleanType> CHIRSerializer::CHIRSerializerImpl::Serialize(const BooleanType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateBooleanType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::UnitType> CHIRSerializer::CHIRSerializerImpl::Serialize(const UnitType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateUnitType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::NothingType> CHIRSerializer::CHIRSerializerImpl::Serialize(const NothingType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateNothingType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::NumericType> CHIRSerializer::CHIRSerializerImpl::Serialize(const NumericType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateNumericType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::TupleType> CHIRSerializer::CHIRSerializerImpl::Serialize(const TupleType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateTupleType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::ClosureType> CHIRSerializer::CHIRSerializerImpl::Serialize(const ClosureType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateClosureType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::RawArrayType> CHIRSerializer::CHIRSerializerImpl::Serialize(const RawArrayType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    auto dims = obj.GetDims();
    return PackageFormat::CreateRawArrayType(builder, base, dims);
}

template <>
flatbuffers::Offset<PackageFormat::VArrayType> CHIRSerializer::CHIRSerializerImpl::Serialize(const VArrayType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    auto size = obj.GetSize();
    return PackageFormat::CreateVArrayType(builder, base, size);
}

template <>
flatbuffers::Offset<PackageFormat::FuncType> CHIRSerializer::CHIRSerializerImpl::Serialize(const FuncType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    auto isCFuncType = obj.IsCFunc();
    auto hasVarArg = obj.HasVarArg();
    return PackageFormat::CreateFuncType(builder, base, isCFuncType, hasVarArg);
}

template <>
flatbuffers::Offset<PackageFormat::CustomType> CHIRSerializer::CHIRSerializerImpl::Serialize(const CustomType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    auto customTypeDef = GetId<CustomTypeDef>(obj.GetCustomTypeDef());
    return PackageFormat::CreateCustomType(builder, base, customTypeDef);
}

template <>
flatbuffers::Offset<PackageFormat::CStringType> CHIRSerializer::CHIRSerializerImpl::Serialize(const CStringType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateCStringType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::CPointerType> CHIRSerializer::CHIRSerializerImpl::Serialize(const CPointerType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateCPointerType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::GenericType> CHIRSerializer::CHIRSerializerImpl::Serialize(const GenericType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    auto upperBounds = GetId<Type>(obj.GetUpperBounds());
    return PackageFormat::CreateGenericTypeDirect(builder, base, obj.GetIdentifier().data(), &upperBounds);
}

template <>
flatbuffers::Offset<PackageFormat::RefType> CHIRSerializer::CHIRSerializerImpl::Serialize(const RefType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateRefType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::VoidType> CHIRSerializer::CHIRSerializerImpl::Serialize(const VoidType& obj)
{
    auto base = Serialize<PackageFormat::Type>(static_cast<const Type&>(obj));
    return PackageFormat::CreateVoidType(builder, base);
}

// ========================== Numeric Type Serializers =========================

template <>
flatbuffers::Offset<PackageFormat::IntType> CHIRSerializer::CHIRSerializerImpl::Serialize(const IntType& obj)
{
    auto base = Serialize<PackageFormat::NumericType>(static_cast<const NumericType&>(obj));
    return PackageFormat::CreateIntType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::FloatType> CHIRSerializer::CHIRSerializerImpl::Serialize(const FloatType& obj)
{
    auto base = Serialize<PackageFormat::NumericType>(static_cast<const NumericType&>(obj));
    return PackageFormat::CreateFloatType(builder, base);
}

// ========================== Custom Type Serializers ==========================

template <>
flatbuffers::Offset<PackageFormat::EnumType> CHIRSerializer::CHIRSerializerImpl::Serialize(const EnumType& obj)
{
    auto base = Serialize<PackageFormat::CustomType>(static_cast<const CustomType&>(obj));
    bool isBoxed = false;
    return PackageFormat::CreateEnumType(builder, base, isBoxed);
}

template <>
flatbuffers::Offset<PackageFormat::StructType> CHIRSerializer::CHIRSerializerImpl::Serialize(const StructType& obj)
{
    auto base = Serialize<PackageFormat::CustomType>(static_cast<const CustomType&>(obj));
    return PackageFormat::CreateStructType(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::ClassType> CHIRSerializer::CHIRSerializerImpl::Serialize(const ClassType& obj)
{
    auto base = Serialize<PackageFormat::CustomType>(static_cast<const CustomType&>(obj));
    return PackageFormat::CreateClassType(builder, base);
}

// ======================= Value Serializers ===================================

template <> flatbuffers::Offset<PackageFormat::Value> CHIRSerializer::CHIRSerializerImpl::Serialize(const Value& obj)
{
    auto base = Serialize<PackageFormat::Base>(static_cast<const Base&>(obj));
    auto valueId = GetId<Value>(&obj);
    auto identifier = obj.GetIdentifierWithoutPrefix();
    if (obj.GetValueKind() == Value::ValueKind::KIND_BLOCK_GROUP) {
        identifier = obj.GetIdentifier();
    }
    auto type = GetId<Type>(obj.GetType());
    auto kind = PackageFormat::ValueKind(obj.GetValueKind());
    auto user = GetId<Expression>(obj.GetUsers());
    auto attributes = obj.GetAttributeInfo().GetRawAttrs().to_ulong();
    return PackageFormat::CreateValueDirect(builder, base, type, identifier.data(), kind, valueId, &user, attributes);
}

template <>
flatbuffers::Offset<PackageFormat::Parameter> CHIRSerializer::CHIRSerializerImpl::Serialize(const Parameter& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto ownedFunc = GetId<Value>(obj.GetParentFunc());
    auto ownedLambda = GetId<Expression>(obj.GetOwnerLambda());
    auto associatedDebug = GetId<Expression>(obj.GetDebugExpr());
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo());
    return PackageFormat::CreateParameter(builder, base, ownedFunc, ownedLambda, associatedDebug, annoInfo);
}

template <>
flatbuffers::Offset<PackageFormat::LocalVar> CHIRSerializer::CHIRSerializerImpl::Serialize(const LocalVar& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto associatedExpr = GetId<Expression>(obj.GetExpr());
    auto isRetVal = obj.IsRetValue();
    auto debug = GetId<Expression>(obj.GetDebugExpr());
    // UG need fix here
    auto accurateEnvObjTy = GetId<Type>(nullptr);
    return PackageFormat::CreateLocalVar(builder, base, associatedExpr, isRetVal, debug, accurateEnvObjTy);
}

template <>
flatbuffers::Offset<PackageFormat::GlobalVar> CHIRSerializer::CHIRSerializerImpl::Serialize(const GlobalVar& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto srcCodeIdentifier = obj.GetIdentifierWithoutPrefix();
    auto packageName = obj.GetPackageName();
    auto rawMangledName = obj.GetRawMangledName();
    auto defaultInitVal = GetId<Value>(obj.GetInitializer());
    auto associatedInitFunc = GetId<Value>(obj.GetInitFunc());
    auto declaredParent = GetId<CustomTypeDef>(obj.GetParentCustomTypeDef());
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo());
    return PackageFormat::CreateGlobalVarDirect(builder, base, rawMangledName.data(), srcCodeIdentifier.data(),
        packageName.data(), defaultInitVal, associatedInitFunc, declaredParent, annoInfo);
}

template <> flatbuffers::Offset<PackageFormat::Block> CHIRSerializer::CHIRSerializerImpl::Serialize(const Block& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto parentGroup = GetId<Value>(obj.GetParentBlockGroup());
    auto exprs = GetId<Expression>(obj.GetExpressions());
    auto predecessors = GetId<Value>(obj.GetPredecessors());
    auto exceptionCatchList = GetId<Type>(obj.IsLandingPadBlock() ? obj.GetExceptions() : std::vector<ClassType*>());
    unsigned predecessorTerminal = 0;
    return PackageFormat::CreateBlockDirect(builder, base, parentGroup, &exprs, &predecessors, obj.IsLandingPadBlock(),
        &exceptionCatchList, predecessorTerminal);
}

template <>
flatbuffers::Offset<PackageFormat::BlockGroup> CHIRSerializer::CHIRSerializerImpl::Serialize(const BlockGroup& obj)
{
    CJC_ASSERT(obj.GetOwnerFunc() || obj.GetOwnerExpression());
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto entryBlock = GetId<Value>(obj.GetEntryBlock());
    auto blocks = GetId<Value>(obj.GetBlocks());
    auto ownedFunc = GetId<Value>(obj.GetOwnerFunc());
    auto ownedLambda = GetId<Expression>(obj.GetOwnerExpression());
    return PackageFormat::CreateBlockGroupDirect(builder, base, entryBlock, &blocks, ownedFunc, ownedLambda);
}

template <> flatbuffers::Offset<PackageFormat::Func> CHIRSerializer::CHIRSerializerImpl::Serialize(const Func& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    auto packageName = obj.GetPackageName();
    auto body = GetId<Value>(obj.GetBody());
    auto params = GetId<Value>(obj.GetParams());
    auto funcKind = PackageFormat::FuncKind(obj.GetFuncKind());
    auto genericTypeParams = GetId<Type>(obj.GetGenericTypeParams());
    auto retVal = GetId<Value>(obj.GetReturnValue());
    auto rawMangledName = obj.GetRawMangledName();
    auto declaredParent = GetId<CustomTypeDef>(obj.GetParentCustomTypeDef());
    auto extendedParent = GetId<CustomTypeDef>(obj.GetOuterDeclaredOrExtendedDef());
    auto genericDecl = GetId<Value>(obj.GetGenericDecl());
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo());
    auto parentName = obj.GetParentRawMangledName();
    auto propLoc = Serialize<PackageFormat::DebugLocation>(obj.GetPropLocation());
    auto ownerFunc = GetId<Value>(obj.GetParamDftValHostFunc());
    CJC_ASSERT(obj.GetBody());
    return PackageFormat::CreateFuncDirect(builder, base, srcCodeIdentifier.data(), packageName.data(), body, &params,
        funcKind, &genericTypeParams, retVal, rawMangledName.data(), declaredParent, extendedParent, genericDecl,
        annoInfo, parentName.data(), propLoc, obj.IsFastNative(), obj.IsCFFIWrapper(), ownerFunc);
}

template <>
flatbuffers::Offset<PackageFormat::ImportedValue> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const ImportedValue& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto packageName = obj.GetSourcePackageName();
    return PackageFormat::CreateImportedValueDirect(builder, base, packageName.data());
}

template <>
flatbuffers::Offset<PackageFormat::LiteralValue> CHIRSerializer::CHIRSerializerImpl::Serialize(const LiteralValue& obj)
{
    auto base = Serialize<PackageFormat::Value>(static_cast<const Value&>(obj));
    auto literalKind = PackageFormat::ConstantValueKind(obj.GetConstantValueKind());
    return PackageFormat::CreateLiteralValue(builder, base, literalKind);
}

// ======================= Imported Value Serializers ===========================

template <>
flatbuffers::Offset<PackageFormat::ImportedFunc> CHIRSerializer::CHIRSerializerImpl::Serialize(const ImportedFunc& obj)
{
    auto base = Serialize<PackageFormat::ImportedValue>(static_cast<const ImportedValue&>(obj));
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    auto funcKind = PackageFormat::FuncKind(obj.GetFuncKind());
    auto genericTypeParams = GetId<Type>(obj.GetGenericTypeParams());
    auto rawMangledName = obj.GetRawMangledName();
    auto packageName = obj.GetPackageName();
    auto paramInfo = SerializeVec<PackageFormat::AbstractMethodParam>(obj.GetParamInfo());
    auto declaredParent = GetId<CustomTypeDef>(obj.GetParentCustomTypeDef());
    auto extendedParent = GetId<CustomTypeDef>(obj.GetOuterDeclaredOrExtendedDef());
    auto genericDecl = GetId<Value>(obj.GetGenericDecl());
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo());
    return PackageFormat::CreateImportedFuncDirect(builder, base, srcCodeIdentifier.data(), funcKind,
        &genericTypeParams, false, 0, rawMangledName.data(), packageName.data(),
        &paramInfo, declaredParent, extendedParent, genericDecl, annoInfo, obj.IsFastNative(), obj.IsCFFIWrapper());
}

template <>
flatbuffers::Offset<PackageFormat::ImportedVar> CHIRSerializer::CHIRSerializerImpl::Serialize(const ImportedVar& obj)
{
    auto base = Serialize<PackageFormat::ImportedValue>(static_cast<const ImportedValue&>(obj));
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    auto annoInfo = Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo());
    return PackageFormat::CreateImportedVarDirect(builder, base, srcCodeIdentifier.data(), annoInfo);
}

// ======================= Literal Value Serializers ===========================
template <>
flatbuffers::Offset<PackageFormat::BoolLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const BoolLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    auto val = obj.GetVal();
    return PackageFormat::CreateBoolLiteral(builder, base, val);
}

template <>
flatbuffers::Offset<PackageFormat::RuneLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const RuneLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    auto val = obj.GetVal();
    return PackageFormat::CreateRuneLiteral(builder, base, val);
}

template <>
flatbuffers::Offset<PackageFormat::StringLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const StringLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    auto val = builder.CreateSharedString(obj.GetVal());
    return PackageFormat::CreateStringLiteral(builder, base, val, false);
}

template <>
flatbuffers::Offset<PackageFormat::IntLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const IntLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    auto val = obj.GetUnsignedVal();
    return PackageFormat::CreateIntLiteral(builder, base, val);
}

template <>
flatbuffers::Offset<PackageFormat::FloatLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const FloatLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    auto val = obj.GetVal();
    return PackageFormat::CreateFloatLiteral(builder, base, val);
}

template <>
flatbuffers::Offset<PackageFormat::UnitLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const UnitLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    return PackageFormat::CreateUnitLiteral(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::NullLiteral> CHIRSerializer::CHIRSerializerImpl::Serialize(const NullLiteral& obj)
{
    auto base = Serialize<PackageFormat::LiteralValue>(static_cast<const LiteralValue&>(obj));
    return PackageFormat::CreateNullLiteral(builder, base);
}

// ======================= Expression Serializers ==============================
template <>
flatbuffers::Offset<PackageFormat::Expression> CHIRSerializer::CHIRSerializerImpl::Serialize(const Expression& obj)
{
    auto base = Serialize<PackageFormat::Base>(static_cast<const Base&>(obj));
    auto expressionId = GetId<Expression>(&obj);
    auto kind = PackageFormat::CHIRExprKind(obj.GetExprKind());
    auto operands = GetId<Value>(obj.GetOperands());
    auto blockGroups = GetId<Value>(obj.GetBlockGroups());
    auto parentBlock = GetId<Value>(obj.GetParent());
    auto resultLocalVar = GetId<Value>(obj.GetResult());
    auto resultTy = GetId<Type>(obj.GetResult() ? obj.GetResult()->GetType() : nullptr);
    return PackageFormat::CreateExpressionDirect(
        builder, base, kind, expressionId, &operands, &blockGroups, parentBlock, resultLocalVar, resultTy);
}

template <>
flatbuffers::Offset<PackageFormat::UnaryExpression> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const UnaryExpression& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto overflowStrategy = PackageFormat::OverflowStrategy(obj.GetOverflowStrategy());
    return PackageFormat::CreateUnaryExpression(builder, base, overflowStrategy);
}

template <>
flatbuffers::Offset<PackageFormat::BinaryExpression> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const BinaryExpression& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto overflowStrategy = PackageFormat::OverflowStrategy(obj.GetOverflowStrategy());
    return PackageFormat::CreateBinaryExpression(builder, base, overflowStrategy);
}

template <>
flatbuffers::Offset<PackageFormat::Constant> CHIRSerializer::CHIRSerializerImpl::Serialize(const Constant& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto genericArgs = GetId<Type>(obj.GetGenericArgs());
    return PackageFormat::CreateConstantDirect(builder, base, &genericArgs);
}

template <>
flatbuffers::Offset<PackageFormat::Allocate> CHIRSerializer::CHIRSerializerImpl::Serialize(const Allocate& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto targetType = GetId<Type>(obj.GetType());
    return PackageFormat::CreateAllocate(builder, base, targetType);
}

template <> flatbuffers::Offset<PackageFormat::Load> CHIRSerializer::CHIRSerializerImpl::Serialize(const Load& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateLoad(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::Store> CHIRSerializer::CHIRSerializerImpl::Serialize(const Store& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateStore(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::GetElementRef> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const GetElementRef& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto path = obj.GetPath();
    return PackageFormat::CreateGetElementRefDirect(builder, base, &path);
}

template <>
flatbuffers::Offset<PackageFormat::StoreElementRef> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const StoreElementRef& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto path = obj.GetPath();
    return PackageFormat::CreateStoreElementRefDirect(builder, base, &path);
}

template <> flatbuffers::Offset<PackageFormat::Apply> CHIRSerializer::CHIRSerializerImpl::Serialize(const Apply& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto genericArgs = std::vector<uint32_t>(); // lack of getter interface
    auto isSuperCall = obj.IsSuperCall();
    return PackageFormat::CreateApplyDirect(builder, base, &genericArgs, isSuperCall);
}

template <> flatbuffers::Offset<PackageFormat::Invoke> CHIRSerializer::CHIRSerializerImpl::Serialize(const Invoke& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto methodName = obj.GetMethodName();
    auto methodType = GetId<Type>(obj.GetMethodType());
    return PackageFormat::CreateInvokeDirect(builder, base, methodName.data(), methodType);
}

template <>
flatbuffers::Offset<PackageFormat::InvokeStatic> CHIRSerializer::CHIRSerializerImpl::Serialize(const InvokeStatic& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto methodName = obj.GetMethodName();
    auto methodType = GetId<Type>(obj.GetMethodType());
    auto classType = GetId<Type>(obj.GetInstParentCustomTyOfCallee());
    return PackageFormat::CreateInvokeStaticDirect(builder, base, methodName.data(), methodType, classType);
}

template <>
flatbuffers::Offset<PackageFormat::TypeCast> CHIRSerializer::CHIRSerializerImpl::Serialize(const TypeCast& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto targetType = GetId<Type>(obj.GetTargetTy());
    auto overflowStrategy = PackageFormat::OverflowStrategy(obj.GetOverflowStrategy());
    return PackageFormat::CreateTypeCast(builder, base, targetType, overflowStrategy);
}

template <>
flatbuffers::Offset<PackageFormat::InstanceOf> CHIRSerializer::CHIRSerializerImpl::Serialize(const InstanceOf& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto targetType = GetId<Type>(obj.GetType());
    return PackageFormat::CreateInstanceOf(builder, base, targetType);
}

template <> flatbuffers::Offset<PackageFormat::Box> CHIRSerializer::CHIRSerializerImpl::Serialize(const Box& expr)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(expr));
    return PackageFormat::CreateBox(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::UnBox> CHIRSerializer::CHIRSerializerImpl::Serialize(const UnBox& expr)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(expr));
    return PackageFormat::CreateUnBox(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::Terminator> CHIRSerializer::CHIRSerializerImpl::Serialize(const Terminator& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto successors = GetId<Value>(obj.GetSuccessors());
    return PackageFormat::CreateTerminatorDirect(builder, base, &successors);
}

template <> flatbuffers::Offset<PackageFormat::GoTo> CHIRSerializer::CHIRSerializerImpl::Serialize(const GoTo& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    return PackageFormat::CreateGoTo(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::Branch> CHIRSerializer::CHIRSerializerImpl::Serialize(const Branch& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto sourceExpr = obj.sourceExpr;
    return PackageFormat::CreateBranch(builder, base, PackageFormat::SourceExpr(sourceExpr));
}

template <>
flatbuffers::Offset<PackageFormat::MultiBranch> CHIRSerializer::CHIRSerializerImpl::Serialize(const MultiBranch& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto caseVals = obj.GetCaseVals();
    return PackageFormat::CreateMultiBranchDirect(builder, base, &caseVals);
}

template <> flatbuffers::Offset<PackageFormat::Exit> CHIRSerializer::CHIRSerializerImpl::Serialize(const Exit& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    return PackageFormat::CreateExit(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::RaiseException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const RaiseException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    return PackageFormat::CreateRaiseException(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::ApplyWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const ApplyWithException& obj)
{
    CJC_ASSERT(!obj.GetOperands().empty());
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto genericArgs = std::vector<uint32_t>(); // lack of getter interface
    return PackageFormat::CreateApplyWithExceptionDirect(builder, base, &genericArgs);
}

template <>
flatbuffers::Offset<PackageFormat::InvokeWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const InvokeWithException& obj)
{
    CJC_ASSERT(!obj.GetOperands().empty());
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto methodName = obj.GetMethodName();
    auto methodType = GetId<Type>(obj.GetMethodType());
    return PackageFormat::CreateInvokeWithExceptionDirect(builder, base, methodName.data(), methodType);
}

template <>
flatbuffers::Offset<PackageFormat::InvokeStaticWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const InvokeStaticWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto methodName = obj.GetMethodName();
    auto methodType = GetId<Type>(obj.GetMethodType());
    auto classType = GetId<Type>(obj.GetInstParentCustomTyOfCallee());
    return PackageFormat::CreateInvokeStaticWithExceptionDirect(
        builder, base, methodName.data(), methodType, classType);
}

template <>
flatbuffers::Offset<PackageFormat::IntOpWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const IntOpWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto opKind = PackageFormat::CHIRExprKind(obj.GetOpKind());
    return PackageFormat::CreateIntOpWithException(builder, base, opKind);
}

template <>
flatbuffers::Offset<PackageFormat::TypeCastWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const TypeCastWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto targetType = GetId<Type>(obj.GetTargetTy());
    return PackageFormat::CreateTypeCastWithException(builder, base, targetType);
}

template <>
flatbuffers::Offset<PackageFormat::IntrinsicWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const IntrinsicWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto intrinsicKind = PackageFormat::IntrinsicKind(obj.GetIntrinsicKind());
    auto genericArgs = GetId<Type>(obj.GetGenericTypeInfo());
    return PackageFormat::CreateIntrinsicWithExceptionDirect(builder, base, intrinsicKind, &genericArgs);
}

template <>
flatbuffers::Offset<PackageFormat::AllocateWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const AllocateWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto targetType = GetId<Type>(obj.GetType());
    return PackageFormat::CreateAllocateWithException(builder, base, targetType);
}

template <>
flatbuffers::Offset<PackageFormat::RawArrayAllocateWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const RawArrayAllocateWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto elementType = GetId<Type>(obj.GetElementType());
    return PackageFormat::CreateRawArrayAllocateWithException(builder, base, elementType);
}

template <>
flatbuffers::Offset<PackageFormat::SpawnWithException> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const SpawnWithException& obj)
{
    auto base = Serialize<PackageFormat::Terminator>(static_cast<const Terminator&>(obj));
    auto isExecuteClosure = obj.IsExecuteClosure();
    auto executeClosure = GetId<Value>(isExecuteClosure ? obj.GetExecuteClosure() : nullptr);
    return PackageFormat::CreateSpawnWithException(builder, base, isExecuteClosure, executeClosure);
}

template <> flatbuffers::Offset<PackageFormat::Tuple> CHIRSerializer::CHIRSerializerImpl::Serialize(const Tuple& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateTuple(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::Field> CHIRSerializer::CHIRSerializerImpl::Serialize(const Field& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto path = obj.GetIndexes();
    return PackageFormat::CreateFieldDirect(builder, base, &path);
}

template <>
flatbuffers::Offset<PackageFormat::RawArrayAllocate> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const RawArrayAllocate& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto elementType = GetId<Type>(obj.GetElementType());
    return PackageFormat::CreateRawArrayAllocate(builder, base, elementType);
}

template <>
flatbuffers::Offset<PackageFormat::RawArrayLiteralInit> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const RawArrayLiteralInit& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateRawArrayLiteralInit(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::RawArrayInitByValue> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const RawArrayInitByValue& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateRawArrayInitByValue(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::VArray> CHIRSerializer::CHIRSerializerImpl::Serialize(const VArray& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateVArray(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::VArrayBd> CHIRSerializer::CHIRSerializerImpl::Serialize(const VArrayBuilder& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateVArrayBd(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::GetException> CHIRSerializer::CHIRSerializerImpl::Serialize(const GetException& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateGetException(builder, base);
}

template <>
flatbuffers::Offset<PackageFormat::Intrinsic> CHIRSerializer::CHIRSerializerImpl::Serialize(const Intrinsic& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto intrinsicKind = PackageFormat::IntrinsicKind(obj.GetIntrinsicKind());
    auto genericArgs = GetId<Type>(obj.GetGenericTypeInfo());
    return PackageFormat::CreateIntrinsicDirect(builder, base, intrinsicKind, &genericArgs);
}

template <> flatbuffers::Offset<PackageFormat::If> CHIRSerializer::CHIRSerializerImpl::Serialize(const If& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateIf(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::Loop> CHIRSerializer::CHIRSerializerImpl::Serialize(const Loop& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateLoop(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::ForInRange> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const ForInRange& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateForInRange(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::ForInIter> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const ForInIter& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    return PackageFormat::CreateForInIter(builder, base);
}

template <> flatbuffers::Offset<PackageFormat::Debug> CHIRSerializer::CHIRSerializerImpl::Serialize(const Debug& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    // need fix
    auto accurateEnvObjTy = GetId<Type>(nullptr);
    CJC_ASSERT(!obj.GetOperands().empty());
    return PackageFormat::CreateDebugDirect(builder, base, srcCodeIdentifier.data(), accurateEnvObjTy);
}

template <> flatbuffers::Offset<PackageFormat::Spawn> CHIRSerializer::CHIRSerializerImpl::Serialize(const Spawn& obj)
{
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto isExecuteClosure = obj.IsExecuteClosure();
    auto executeClosure = GetId<Value>(isExecuteClosure ? obj.GetExecuteClosure() : nullptr);
    return PackageFormat::CreateSpawn(builder, base, isExecuteClosure, executeClosure);
}

template <> flatbuffers::Offset<PackageFormat::Lambda> CHIRSerializer::CHIRSerializerImpl::Serialize(const Lambda& obj)
{
    CJC_ASSERT(obj.GetBlockGroups().size() == 1);
    CJC_ASSERT(obj.GetBody());
    auto base = Serialize<PackageFormat::Expression>(static_cast<const Expression&>(obj));
    auto funcTy = GetId<Type>(obj.GetFuncType()); // use typeID
    auto isLocalFunc = obj.IsLocalFunc();
    auto identifier = obj.GetIdentifier();
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    auto params = GetId<Value>(obj.GetParams());
    auto genericTypeParams = GetId<Type>(obj.GetGenericTypeParams());
    auto capturedVars = GetId<Value>(obj.GetCapturedVars());
    return PackageFormat::CreateLambdaDirect(builder, base, funcTy, isLocalFunc, identifier.data(),
        srcCodeIdentifier.data(), &params, &genericTypeParams, &capturedVars);
}

// ======================= Custom Type Def Serializers =========================

template <>
flatbuffers::Offset<PackageFormat::CustomTypeDef> CHIRSerializer::CHIRSerializerImpl::Serialize(
    const CustomTypeDef& obj)
{
    auto base = Serialize<PackageFormat::Base>(static_cast<const Base&>(obj));
    auto customTypeDefId = GetId<CustomTypeDef>(&obj);
    auto kind = PackageFormat::CustomTypeKind(obj.GetCustomKind());
    auto identifier = obj.GetIdentifierWithoutPrefix();
    auto srcCodeIdentifier = obj.GetSrcCodeIdentifier();
    auto packageName = obj.GetPackageName();
    auto type = GetId<Type>(obj.GetType());
    auto genericTypeParams = GetId<Type>(obj.GetGenericTypeParams());
    auto methods = GetId<Value>(obj.GetMethods());
    auto instanceMemberVars = obj.GetCustomKind() == CustomDefKind::TYPE_CLASS
        ? SerializeVec<PackageFormat::MemberVarInfo>(StaticCast<const ClassDef&>(obj).GetDirectInstanceVars())
        : SerializeVec<PackageFormat::MemberVarInfo>(obj.GetAllInstanceVars());
    auto staticMemberVars = GetId<Value>(obj.GetStaticMemberVars());
    auto attributes = obj.GetAttributeInfo().GetRawAttrs().to_ulong();
    auto implementedInterfaces = GetId<CustomTypeDef>(obj.GetImplementedInterfaceDefs());
    auto extendedInterfaces = GetId<CustomTypeDef>(GetExtendedInterfaceDefs(obj));
    // UG need implement this again
    auto extendedMethods = std::vector<uint32_t>();
    return PackageFormat::CreateCustomTypeDefDirect(builder, base, kind, identifier.data(), srcCodeIdentifier.data(),
        packageName.data(), customTypeDefId, type, &genericTypeParams, &methods, &instanceMemberVars, &staticMemberVars,
        attributes, &implementedInterfaces, &extendedInterfaces, &extendedMethods,
        GetId<CustomTypeDef>(obj.GetGenericDecl()), Serialize<PackageFormat::AnnoInfo>(obj.GetAnnoInfo()));
}

template <>
flatbuffers::Offset<PackageFormat::EnumDef> CHIRSerializer::CHIRSerializerImpl::Serialize(const EnumDef& obj)
{
    auto base = Serialize<PackageFormat::CustomTypeDef>(static_cast<const CustomTypeDef&>(obj));
    auto ctors = SerializeVec<PackageFormat::EnumCtorInfo>(obj.GetCtors());
    auto nonExhaustive = !obj.IsExhaustive();
    return PackageFormat::CreateEnumDefDirect(builder, base, &ctors, nonExhaustive);
}

template <>
flatbuffers::Offset<PackageFormat::StructDef> CHIRSerializer::CHIRSerializerImpl::Serialize(const StructDef& obj)
{
    auto base = Serialize<PackageFormat::CustomTypeDef>(static_cast<const CustomTypeDef&>(obj));
    auto isCStruct = obj.IsCStruct();
    return PackageFormat::CreateStructDef(builder, base, isCStruct);
}

template <>
flatbuffers::Offset<PackageFormat::ClassDef> CHIRSerializer::CHIRSerializerImpl::Serialize(const ClassDef& obj)
{
    auto base = Serialize<PackageFormat::CustomTypeDef>(static_cast<const CustomTypeDef&>(obj));
    auto kind = obj.IsInterface() ? PackageFormat::ClassDefKind::ClassDefKind_INTERFACE
                                  : PackageFormat::ClassDefKind::ClassDefKind_CLASS;
    auto isAnnotation = obj.IsAnnotation();
    auto superClass = GetId<Type>(obj.GetSuperClassTy());

    return PackageFormat::CreateClassDefDirect(builder, base, kind, isAnnotation, superClass, 0);
}

// ========================== Dispatchers ===============================

template <> flatbuffers::Offset<void> CHIRSerializer::CHIRSerializerImpl::Dispatch(const Type& obj)
{
    switch (obj.GetTypeKind()) {
        case Type::TypeKind::TYPE_INT8:
        case Type::TypeKind::TYPE_INT16:
        case Type::TypeKind::TYPE_INT32:
        case Type::TypeKind::TYPE_INT64:
        case Type::TypeKind::TYPE_INT_NATIVE:
        case Type::TypeKind::TYPE_UINT8:
        case Type::TypeKind::TYPE_UINT16:
        case Type::TypeKind::TYPE_UINT32:
        case Type::TypeKind::TYPE_UINT64:
        case Type::TypeKind::TYPE_UINT_NATIVE:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_IntType);
            return Serialize<PackageFormat::IntType>(static_cast<const IntType&>(obj)).Union();
        case Type::TypeKind::TYPE_FLOAT16:
        case Type::TypeKind::TYPE_FLOAT32:
        case Type::TypeKind::TYPE_FLOAT64:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_FloatType);
            return Serialize<PackageFormat::FloatType>(static_cast<const FloatType&>(obj)).Union();
        case Type::TypeKind::TYPE_RUNE:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_RuneType);
            return Serialize<PackageFormat::RuneType>(static_cast<const RuneType&>(obj)).Union();
        case Type::TypeKind::TYPE_BOOLEAN:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_BooleanType);
            return Serialize<PackageFormat::BooleanType>(static_cast<const BooleanType&>(obj)).Union();
        case Type::TypeKind::TYPE_UNIT:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_UnitType);
            return Serialize<PackageFormat::UnitType>(static_cast<const UnitType&>(obj)).Union();
        case Type::TypeKind::TYPE_NOTHING:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_NothingType);
            return Serialize<PackageFormat::NothingType>(static_cast<const NothingType&>(obj)).Union();
        case Type::TypeKind::TYPE_VOID:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_VoidType);
            return Serialize<PackageFormat::VoidType>(static_cast<const VoidType&>(obj)).Union();
        case Type::TypeKind::TYPE_TUPLE:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_TupleType);
            return Serialize<PackageFormat::TupleType>(static_cast<const TupleType&>(obj)).Union();
        case Type::TypeKind::TYPE_CLOSURE:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_ClosureType);
            return Serialize<PackageFormat::ClosureType>(static_cast<const ClosureType&>(obj)).Union();
        case Type::TypeKind::TYPE_STRUCT:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_StructType);
            return Serialize<PackageFormat::StructType>(static_cast<const StructType&>(obj)).Union();
        case Type::TypeKind::TYPE_ENUM:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_EnumType);
            return Serialize<PackageFormat::EnumType>(static_cast<const EnumType&>(obj)).Union();
        case Type::TypeKind::TYPE_FUNC:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_FuncType);
            return Serialize<PackageFormat::FuncType>(static_cast<const FuncType&>(obj)).Union();
        case Type::TypeKind::TYPE_CLASS:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_ClassType);
            return Serialize<PackageFormat::ClassType>(static_cast<const ClassType&>(obj)).Union();
        case Type::TypeKind::TYPE_RAWARRAY:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_RawArrayType);
            return Serialize<PackageFormat::RawArrayType>(static_cast<const RawArrayType&>(obj)).Union();
        case Type::TypeKind::TYPE_VARRAY:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_VArrayType);
            return Serialize<PackageFormat::VArrayType>(static_cast<const VArrayType&>(obj)).Union();
        case Type::TypeKind::TYPE_CPOINTER:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_CPointerType);
            return Serialize<PackageFormat::CPointerType>(static_cast<const CPointerType&>(obj)).Union();
        case Type::TypeKind::TYPE_CSTRING:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_CStringType);
            return Serialize<PackageFormat::CStringType>(static_cast<const CStringType&>(obj)).Union();
        case Type::TypeKind::TYPE_GENERIC:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_GenericType);
            return Serialize<PackageFormat::GenericType>(static_cast<const GenericType&>(obj)).Union();
        case Type::TypeKind::TYPE_REFTYPE:
            typeKind[GetId<Type>(&obj) - 1] = static_cast<uint8_t>(PackageFormat::TypeElem_RefType);
            return Serialize<PackageFormat::RefType>(static_cast<const RefType&>(obj)).Union();
        case Type::TypeKind::TYPE_BOXTYPE:
            // UG need fix here
            return 0;
        case Type::TypeKind::TYPE_THIS:
            // UG need fix here
            return 0;
        case Type::TypeKind::TYPE_INVALID:
        case Type::TypeKind::MAX_TYPE_KIND:
            InternalError("unsupported type kind in chir serialization");
            return 0;
    }
}

template <> flatbuffers::Offset<void> CHIRSerializer::CHIRSerializerImpl::Dispatch(const LiteralValue& obj)
{
    switch (obj.GetConstantValueKind()) {
        case ConstantValueKind::KIND_BOOL:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_BoolLiteral;
            return Serialize<PackageFormat::BoolLiteral>(static_cast<const BoolLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_RUNE:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_RuneLiteral;
            return Serialize<PackageFormat::RuneLiteral>(static_cast<const RuneLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_INT:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_IntLiteral;
            return Serialize<PackageFormat::IntLiteral>(static_cast<const IntLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_FLOAT:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_FloatLiteral;
            return Serialize<PackageFormat::FloatLiteral>(static_cast<const FloatLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_STRING:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_StringLiteral;
            return Serialize<PackageFormat::StringLiteral>(static_cast<const StringLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_UNIT:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_UnitLiteral;
            return Serialize<PackageFormat::UnitLiteral>(static_cast<const UnitLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_NULL:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_NullLiteral;
            return Serialize<PackageFormat::NullLiteral>(static_cast<const NullLiteral&>(obj)).Union();
        case ConstantValueKind::KIND_FUNC:
            return 0;
    }
}

template <> flatbuffers::Offset<void> CHIRSerializer::CHIRSerializerImpl::Dispatch(const Value& obj)
{
    switch (obj.GetValueKind()) {
        case Value::ValueKind::KIND_LITERAL:
            return Dispatch<LiteralValue>(static_cast<const LiteralValue&>(obj)).Union();
        case Value::ValueKind::KIND_GLOBALVAR:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_GlobalVar;
            return Serialize<PackageFormat::GlobalVar>(dynamic_cast<const GlobalVar&>(obj)).Union();
        case Value::ValueKind::KIND_PARAMETER:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_Parameter;
            return Serialize<PackageFormat::Parameter>(static_cast<const Parameter&>(obj)).Union();
        case Value::ValueKind::KIND_IMP_FUNC:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_ImportedFunc;
            return Serialize<PackageFormat::ImportedFunc>(dynamic_cast<const ImportedFunc&>(obj)).Union();
        case Value::ValueKind::KIND_IMP_VAR:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_ImportedVar;
            return Serialize<PackageFormat::ImportedVar>(dynamic_cast<const ImportedVar&>(obj)).Union();
        case Value::ValueKind::KIND_LOCALVAR:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_LocalVar;
            return Serialize<PackageFormat::LocalVar>(static_cast<const LocalVar&>(obj)).Union();
        case Value::ValueKind::KIND_FUNC:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_Func;
            return Serialize<PackageFormat::Func>(dynamic_cast<const Func&>(obj)).Union();
        case Value::ValueKind::KIND_BLOCK:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_Block;
            return Serialize<PackageFormat::Block>(static_cast<const Block&>(obj)).Union();
        case Value::ValueKind::KIND_BLOCK_GROUP:
            valueKind[GetId<Value>(&obj) - 1] = PackageFormat::ValueElem_BlockGroup;
            return Serialize<PackageFormat::BlockGroup>(static_cast<const BlockGroup&>(obj)).Union();
    }
}

template <> flatbuffers::Offset<void> CHIRSerializer::CHIRSerializerImpl::Dispatch(const Expression& obj)
{
    switch (obj.GetExprKind()) {
        case ExprKind::GOTO:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_GoTo;
            return Serialize<PackageFormat::GoTo>(static_cast<const GoTo&>(obj)).Union();
        case ExprKind::BRANCH:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Branch;
            return Serialize<PackageFormat::Branch>(static_cast<const Branch&>(obj)).Union();
        case ExprKind::MULTIBRANCH:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_MultiBranch;
            return Serialize<PackageFormat::MultiBranch>(static_cast<const MultiBranch&>(obj)).Union();
        case ExprKind::EXIT:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Exit;
            return Serialize<PackageFormat::Exit>(static_cast<const Exit&>(obj)).Union();
        case ExprKind::APPLY_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_ApplyWithException;
            return Serialize<PackageFormat::ApplyWithException>(static_cast<const ApplyWithException&>(obj)).Union();
        case ExprKind::INVOKE_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_InvokeWithException;
            return Serialize<PackageFormat::InvokeWithException>(static_cast<const InvokeWithException&>(obj)).Union();
        case ExprKind::RAISE_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_RaiseException;
            return Serialize<PackageFormat::RaiseException>(static_cast<const RaiseException&>(obj)).Union();
        case ExprKind::INT_OP_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_IntOpWithException;
            return Serialize<PackageFormat::IntOpWithException>(static_cast<const IntOpWithException&>(obj)).Union();
        case ExprKind::SPAWN_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_SpawnWithException;
            return Serialize<PackageFormat::SpawnWithException>(static_cast<const SpawnWithException&>(obj)).Union();
        case ExprKind::TYPECAST_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_TypeCastWithException;
            return Serialize<PackageFormat::TypeCastWithException>(static_cast<const TypeCastWithException&>(obj))
                .Union();
        case ExprKind::INTRINSIC_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_IntrinsicWithException;
            return Serialize<PackageFormat::IntrinsicWithException>(static_cast<const IntrinsicWithException&>(obj))
                .Union();
        case ExprKind::ALLOCATE_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_AllocateWithException;
            return Serialize<PackageFormat::AllocateWithException>(static_cast<const AllocateWithException&>(obj))
                .Union();
        case ExprKind::INVOKESTATIC_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_InvokeStaticWithException;
            return Serialize<PackageFormat::InvokeStaticWithException>(
                static_cast<const InvokeStaticWithException&>(obj))
                .Union();
        case ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_RawArrayAllocateWithException;
            return Serialize<PackageFormat::RawArrayAllocateWithException>(
                static_cast<const RawArrayAllocateWithException&>(obj))
                .Union();
        case ExprKind::NEG:
        case ExprKind::NOT:
        case ExprKind::BITNOT:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_UnaryExpression;
            return Serialize<PackageFormat::UnaryExpression>(static_cast<const UnaryExpression&>(obj)).Union();
        case ExprKind::ADD:
        case ExprKind::SUB:
        case ExprKind::MUL:
        case ExprKind::DIV:
        case ExprKind::MOD:
        case ExprKind::EXP:
        case ExprKind::LSHIFT:
        case ExprKind::RSHIFT:
        case ExprKind::BITAND:
        case ExprKind::BITOR:
        case ExprKind::BITXOR:
        case ExprKind::LT:
        case ExprKind::GT:
        case ExprKind::LE:
        case ExprKind::GE:
        case ExprKind::EQUAL:
        case ExprKind::NOTEQUAL:
        case ExprKind::AND:
        case ExprKind::OR:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_BinaryExpression;
            return Serialize<PackageFormat::BinaryExpression>(static_cast<const BinaryExpression&>(obj)).Union();
        case ExprKind::ALLOCATE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Allocate;
            return Serialize<PackageFormat::Allocate>(static_cast<const Allocate&>(obj)).Union();
        case ExprKind::LOAD:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Load;
            return Serialize<PackageFormat::Load>(static_cast<const Load&>(obj)).Union();
        case ExprKind::STORE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Store;
            return Serialize<PackageFormat::Store>(static_cast<const Store&>(obj)).Union();
        case ExprKind::GET_ELEMENT_REF:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_GetElementRef;
            return Serialize<PackageFormat::GetElementRef>(static_cast<const GetElementRef&>(obj)).Union();
        case ExprKind::STORE_ELEMENT_REF:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_StoreElementRef;
            return Serialize<PackageFormat::StoreElementRef>(static_cast<const StoreElementRef&>(obj)).Union();
        case ExprKind::IF:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_If;
            return Serialize<PackageFormat::If>(static_cast<const If&>(obj)).Union();
        case ExprKind::LOOP:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Loop;
            return Serialize<PackageFormat::Loop>(static_cast<const Loop&>(obj)).Union();
        case ExprKind::FORIN_RANGE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_ForInRange;
            return Serialize<PackageFormat::ForInRange>(static_cast<const ForInRange&>(obj)).Union();
        case ExprKind::FORIN_ITER:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_ForInIter;
            return Serialize<PackageFormat::ForInIter>(static_cast<const ForInIter&>(obj)).Union();
        case ExprKind::FORIN_CLOSED_RANGE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_ForInClosedRange;
            return Serialize<PackageFormat::ForInIter>(static_cast<const ForInIter&>(obj)).Union();
        case ExprKind::LAMBDA:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Lambda;
            return Serialize<PackageFormat::Lambda>(static_cast<const Lambda&>(obj)).Union();
        case ExprKind::CONSTANT:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Constant;
            return Serialize<PackageFormat::Constant>(static_cast<const Constant&>(obj)).Union();
        case ExprKind::DEBUGEXPR:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Debug;
            return Serialize<PackageFormat::Debug>(static_cast<const Debug&>(obj)).Union();
        case ExprKind::TUPLE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Tuple;
            return Serialize<PackageFormat::Tuple>(static_cast<const Tuple&>(obj)).Union();
        case ExprKind::FIELD:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Field;
            return Serialize<PackageFormat::Field>(static_cast<const Field&>(obj)).Union();
        case ExprKind::APPLY:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Apply;
            return Serialize<PackageFormat::Apply>(static_cast<const Apply&>(obj)).Union();
        case ExprKind::INVOKE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Invoke;
            return Serialize<PackageFormat::Invoke>(static_cast<const Invoke&>(obj)).Union();
        case ExprKind::INVOKESTATIC:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_InvokeStatic;
            return Serialize<PackageFormat::InvokeStatic>(static_cast<const InvokeStatic&>(obj)).Union();
        case ExprKind::INSTANCEOF:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_InstanceOf;
            return Serialize<PackageFormat::InstanceOf>(static_cast<const InstanceOf&>(obj)).Union();
        case ExprKind::BOX:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Box;
            return Serialize<PackageFormat::Box>(static_cast<const Box&>(obj)).Union();
        case ExprKind::UNBOX:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_UnBox;
            return Serialize<PackageFormat::UnBox>(static_cast<const UnBox&>(obj)).Union();
        case ExprKind::TYPECAST:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_TypeCast;
            return Serialize<PackageFormat::TypeCast>(static_cast<const TypeCast&>(obj)).Union();
        case ExprKind::GET_EXCEPTION:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_GetException;
            return Serialize<PackageFormat::GetException>(static_cast<const GetException&>(obj)).Union();
        case ExprKind::RAW_ARRAY_ALLOCATE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_RawArrayAllocate;
            return Serialize<PackageFormat::RawArrayAllocate>(static_cast<const RawArrayAllocate&>(obj)).Union();
        case ExprKind::RAW_ARRAY_LITERAL_INIT:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_RawArrayLiteralInit;
            return Serialize<PackageFormat::RawArrayLiteralInit>(static_cast<const RawArrayLiteralInit&>(obj)).Union();
        case ExprKind::RAW_ARRAY_INIT_BY_VALUE:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_RawArrayInitByValue;
            return Serialize<PackageFormat::RawArrayInitByValue>(static_cast<const RawArrayInitByValue&>(obj)).Union();
        case ExprKind::VARRAY:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_VArray;
            return Serialize<PackageFormat::VArray>(static_cast<const VArray&>(obj)).Union();
        case ExprKind::VARRAY_BUILDER:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_VArrayBd;
            return Serialize<PackageFormat::VArrayBd>(static_cast<const VArrayBuilder&>(obj)).Union();
        case ExprKind::INTRINSIC:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Intrinsic;
            return Serialize<PackageFormat::Intrinsic>(static_cast<const Intrinsic&>(obj)).Union();
        case ExprKind::SPAWN:
            exprKind[GetId<Expression>(&obj) - 1] = PackageFormat::ExpressionElem_Spawn;
            return Serialize<PackageFormat::Spawn>(static_cast<const Spawn&>(obj)).Union();
        case ExprKind::INVALID:
        case ExprKind::MAX_EXPR_KINDS:
            InternalError("unsupported expr kind in chir serialization");
            return 0;
        case ExprKind::TRANSFORM_TO_GENERIC:
        case ExprKind::TRANSFORM_TO_CONCRETE:
        case ExprKind::GET_INSTANTIATE_VALUE:
        case ExprKind::UNBOX_TO_REF:
        case ExprKind::GET_RTTI:
        case ExprKind::GET_RTTI_STATIC:
            return 0; // UG need fix
    }
}

template <> flatbuffers::Offset<void> CHIRSerializer::CHIRSerializerImpl::Dispatch(const CustomTypeDef& obj)
{
    switch (obj.GetCustomKind()) {
        case CustomDefKind::TYPE_STRUCT:
            defKind[GetId<CustomTypeDef>(&obj) - 1] = PackageFormat::CustomTypeDefElem_StructDef;
            return Serialize<PackageFormat::StructDef>(static_cast<const StructDef&>(obj)).Union();
        case CustomDefKind::TYPE_ENUM:
            defKind[GetId<CustomTypeDef>(&obj) - 1] = PackageFormat::CustomTypeDefElem_EnumDef;
            return Serialize<PackageFormat::EnumDef>(static_cast<const EnumDef&>(obj)).Union();
        case CustomDefKind::TYPE_CLASS:
            defKind[GetId<CustomTypeDef>(&obj) - 1] = PackageFormat::CustomTypeDefElem_ClassDef;
            return Serialize<PackageFormat::ClassDef>(static_cast<const ClassDef&>(obj)).Union();
        case CustomDefKind::TYPE_EXTEND:
            // UG need fix
            return Serialize<PackageFormat::ClassDef>(static_cast<const ClassDef&>(obj)).Union();
    }
}

void CHIRSerializer::CHIRSerializerImpl::Dispatch()
{
    while (!(typeQueue.empty() && valueQueue.empty() && exprQueue.empty() && defQueue.empty())) {
        while (!typeQueue.empty()) {
            auto type = typeQueue.front();
            allType[GetId<Type>(type) - 1] = Dispatch<Type>(*type);
            typeQueue.pop();
        }
        while (!valueQueue.empty()) {
            auto value = valueQueue.front();
            allValue[GetId<Value>(value) - 1] = Dispatch<Value>(*value);
            valueQueue.pop_front();
        }
        while (!exprQueue.empty()) {
            auto expr = exprQueue.front();
            allExpression[GetId<Expression>(expr) - 1] = Dispatch<Expression>(*expr);
            exprQueue.pop();
        }
        while (!defQueue.empty()) {
            auto def = defQueue.front();
            allCustomTypeDef[GetId<CustomTypeDef>(def) - 1] = Dispatch<CustomTypeDef>(*def);
            defQueue.pop_front();
        }
    }
    globalInitFunc = GetId<Value>(package.GetPackageInitFunc());
}

// ========================== Utilities ==========================================

void CHIRSerializer::CHIRSerializerImpl::Save(const std::string& filename)
{
    auto serializedPackage = PackageFormat::CreateCHIRPackageDirect(builder, package.GetName().c_str(),
        "", PackageAccessLevelToString(package.GetPackageAccessLevel()).c_str(), &typeKind, &allType, &valueKind,
        &allValue, &exprKind, &allExpression, &defKind, &allCustomTypeDef);

    builder.Finish(serializedPackage);
    const uint8_t* buf = builder.GetBufferPointer();
    auto size = builder.GetSize();
    std::ofstream output(filename, std::ios::out | std::ofstream::binary);
    if (!output.is_open()) {
        abort();
    }
    output.write(reinterpret_cast<const char*>(buf), static_cast<long>(size));
    output.close();
}

void CHIRSerializer::CHIRSerializerImpl::Initialize()
{
    // imports
    for (auto value : package.GetImportedVarAndFuncs()) {
        valueQueue.push_back(value);
    }

    for (auto def : package.GetImportedStructs()) {
        defQueue.push_back(def);
    }
    for (auto def : package.GetImportedClasses()) {
        defQueue.push_back(def);
    }
    for (auto def : package.GetImportedEnums()) {
        defQueue.push_back(def);
    }

    // current package
    for (auto value : package.GetGlobalVars()) {
        valueQueue.push_back(value);
    }

    for (auto def : package.GetStructs()) {
        defQueue.push_back(def);
    }
    for (auto def : package.GetClasses()) {
        defQueue.push_back(def);
    }
    for (auto def : package.GetEnums()) {
        defQueue.push_back(def);
    }

    for (auto value : package.GetGlobalFuncs()) {
        valueQueue.push_back(value);
    }

    // allocate def id earlier
    for (auto def : std::as_const(defQueue)) {
        def2Id[def] = ++defCount;
        allCustomTypeDef.emplace_back(0);
        defKind.emplace_back(0);
    }

    // allocate value id earlier
    for (auto obj : std::as_const(valueQueue)) {
        value2Id[obj] = ++valueCount;
        allValue.emplace_back(0);
        valueKind.emplace_back(0);
    }
}
