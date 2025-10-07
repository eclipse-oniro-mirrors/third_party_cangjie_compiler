// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements a translation from CHIR to BCHIR.
 */
#include "cangjie/CHIR/Interpreter/CHIR2BCHIR.h"
#include "cangjie/CHIR/Interpreter/Utils.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Utils.h"

using namespace Cangjie::CHIR;
using namespace Interpreter;

void CHIR2BCHIR::TranslateOthersExpression(Context& ctx, const Expression& expr)
{
    switch (expr.GetExprKind()) {
        case ExprKind::DEBUGEXPR: {
            CJC_ASSERT(false);
            break;
        }
        case ExprKind::CONSTANT: {
            // Nothing to be done here. The literal was already encoded because it is the argument 0
            // of expr.
            break;
        }
        case ExprKind::TUPLE: {
            CJC_ASSERT(expr.GetNumOfOperands() > 0);
            CJC_ASSERT(expr.GetNumOfOperands() <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
            PushOpCodeWithAnnotations(ctx, OpCode::TUPLE, expr, static_cast<unsigned>(expr.GetNumOfOperands()));
            break;
        }
        case ExprKind::FIELD: {
            TranslateField(ctx, expr);
            break;
        }
        case ExprKind::APPLY: {
            CJC_ASSERT(expr.GetNumOfOperands() > 0);
            CJC_ASSERT(expr.GetNumOfOperands() <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
            TranslateApplyExpression(ctx, *StaticCast<const Apply*>(&expr));
            break;
        }
        case ExprKind::INVOKE: {
            TranslateInvoke(ctx, expr);
            break;
        }
        case ExprKind::INSTANCEOF: {
            CJC_ASSERT(expr.GetNumOfOperands() == 1);
            auto instanceOfExpr = StaticCast<const InstanceOf*>(&expr);
            TranslateInstanceOf(ctx, *instanceOfExpr);
            break;
        }
        case ExprKind::TYPECAST: {
            TranslateTypecast(ctx, expr);
            break;
        }
        case ExprKind::INTRINSIC: {
            TranslateIntrinsicExpression(ctx, *StaticCast<const Intrinsic*>(&expr));
            break;
        }
        case ExprKind::GET_EXCEPTION: {
            PushOpCodeWithAnnotations(ctx, OpCode::GET_EXCEPTION, expr);
            break;
        }
        case ExprKind::RAW_ARRAY_ALLOCATE: {
            PushOpCodeWithAnnotations(ctx, OpCode::ALLOCATE_RAW_ARRAY, expr);
            break;
        }
        case ExprKind::RAW_ARRAY_LITERAL_INIT: {
            CJC_ASSERT(expr.GetNumOfOperands() > 0); // array + arguments
            PushOpCodeWithAnnotations(
                ctx, OpCode::RAW_ARRAY_LITERAL_INIT, expr, static_cast<unsigned>(expr.GetNumOfOperands() - 1));
            break;
        }
        case ExprKind::RAW_ARRAY_INIT_BY_VALUE: {
            PushOpCodeWithAnnotations(ctx, OpCode::RAW_ARRAY_INIT_BY_VALUE, expr);
            break;
        }
        case ExprKind::VARRAY: {
            CJC_ASSERT(expr.GetNumOfOperands() < static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
            auto vArraySize = static_cast<Bchir::ByteCodeContent>(expr.GetNumOfOperands());
            PushOpCodeWithAnnotations(ctx, OpCode::VARRAY, expr, vArraySize);
            break;
        }
        case ExprKind::VARRAY_BUILDER: {
            auto vArrayBuilder = StaticCast<const VArrayBuilder*>(&expr);
            TranslateVArrayBuilder(ctx, *vArrayBuilder);
            break;
        }
        case ExprKind::SPAWN: {
            PushOpCodeWithAnnotations<true, true>(ctx, OpCode::SPAWN, expr);
            break;
        }
        case ExprKind::BOX: {
            CJC_ASSERT(expr.GetNumOfOperands() == 1);
            auto boxExpr = StaticCast<const Box*>(&expr);
            TranslateBox(ctx, *boxExpr);
            break;
        }
        case ExprKind::UNBOX: {
            CJC_ASSERT(expr.GetNumOfOperands() == 1);
            PushOpCodeWithAnnotations(ctx, OpCode::UNBOX, expr);
            break;
        }
        case ExprKind::UNBOX_TO_REF: {
            CJC_ASSERT(expr.GetNumOfOperands() == 1);
            PushOpCodeWithAnnotations(ctx, OpCode::UNBOX_REF, expr);
            break;
        }
        case ExprKind::INVOKESTATIC:
        case ExprKind::GET_RTTI:
        case ExprKind::GET_RTTI_STATIC:
        case ExprKind::TRANSFORM_TO_CONCRETE:
        case ExprKind::TRANSFORM_TO_GENERIC: {
            // We currently don't support these operations. If they are reached during interpretation
            // the interpreter will terminate with exception.
            PushOpCodeWithAnnotations(ctx, OpCode::ABORT, expr);
            break;
        }
        default: {
            // unreachable
            CJC_ASSERT(false);
            PushOpCodeWithAnnotations(ctx, OpCode::ABORT, expr);
        }
    }
}

void CHIR2BCHIR::TranslateField(Context& ctx, const Expression& expr)
{
    auto fieldExpr = StaticCast<const Field*>(&expr);
    auto indexes = fieldExpr->GetIndexes();
    CJC_ASSERT(indexes.size() > 0);
    if (indexes.size() == 1) {
        PushOpCodeWithAnnotations(ctx, OpCode::FIELD, expr, static_cast<unsigned>(indexes[0]));
    } else {
        CJC_ASSERT(fieldExpr->GetOperands()[0]->GetType()->IsStruct() ||
            fieldExpr->GetOperands()[0]->GetType()->IsEnum() || fieldExpr->GetOperands()[0]->GetType()->IsTuple());
        PushOpCodeWithAnnotations(ctx, OpCode::FIELD_TPL, expr, static_cast<Bchir::ByteCodeContent>(indexes.size()));
        for (auto i : indexes) {
            ctx.def.Push(static_cast<Bchir::ByteCodeContent>(i));
        }
    }
}

void CHIR2BCHIR::TranslateInvoke(Context& ctx, const Expression& expr)
{
    CJC_ASSERT(expr.GetNumOfOperands() > 0);
    CJC_ASSERT(expr.GetNumOfOperands() <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
    auto invokeExpr = StaticCast<const Invoke*>(&expr);
    auto idx = ctx.def.NextIndex();
    // we dont store mangled name here
    PushOpCodeWithAnnotations<false, true>(
        ctx, OpCode::INVOKE, expr, static_cast<unsigned>(expr.GetNumOfOperands()), 0);
    auto methodName = MangleMethodName<true>(invokeExpr->GetMethodName(), *invokeExpr->GetMethodType());
    ctx.def.AddMangledNameAnnotation(idx, methodName);
}

void CHIR2BCHIR::TranslateTypecast(Context& ctx, const Expression& expr)
{
    CJC_ASSERT(expr.GetNumOfOperands() == 1);
    auto typeCastExpr = StaticCast<const TypeCast*>(&expr);
    auto srcTy = typeCastExpr->GetSourceTy();
    auto dstTy = typeCastExpr->GetTargetTy();
    if (srcTy->IsPrimitive() && dstTy->IsPrimitive()) {
        auto srcTyIdx = srcTy->GetTypeKind();
        auto dstTyIdx = dstTy->GetTypeKind();
        auto overflowStrat = static_cast<Bchir::ByteCodeContent>(typeCastExpr->GetOverflowStrategy());
        PushOpCodeWithAnnotations(ctx, OpCode::TYPECAST, expr, srcTyIdx, dstTyIdx, overflowStrat);
    } else {
        CJC_ASSERT((!srcTy->IsPrimitive() && !dstTy->IsPrimitive()) ||
            (srcTy->IsEnum() && IsEnumSelectorType(*dstTy)) || (IsEnumSelectorType(*srcTy) && dstTy->IsEnum()));
    }
}

void CHIR2BCHIR::TranslateInstanceOf(Context& ctx, const InstanceOf& expr)
{
    auto opIdx = ctx.def.Size();
    CJC_ASSERT(opIdx <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
    PushOpCodeWithAnnotations<false>(ctx, OpCode::INSTANCEOF, expr);
    ctx.def.Push(0); // dummy value, this will be resolved during linking
    if (expr.GetType()->IsRef()) {
        auto refTy = StaticCast<const RefType*>(expr.GetType());
        auto classTy = StaticCast<const ClassType*>(refTy->GetBaseType());
        auto classDef = classTy->GetClassDef();
        ctx.def.AddMangledNameAnnotation(static_cast<unsigned>(opIdx), classDef->GetIdentifierWithoutPrefix());
    } else if (expr.GetType()->IsPrimitive()) {
        auto primitiveClassName = expr.GetType()->ToString();
        ctx.def.AddMangledNameAnnotation(static_cast<Bchir::ByteCodeIndex>(opIdx), primitiveClassName);
    } else {
        auto customTy = StaticCast<const CustomType*>(expr.GetType());
        auto customDef = customTy->GetCustomTypeDef();
        ctx.def.AddMangledNameAnnotation(
            static_cast<Bchir::ByteCodeIndex>(opIdx), customDef->GetIdentifierWithoutPrefix());
    }
}

void CHIR2BCHIR::TranslateBox(Context& ctx, const Box& expr)
{
    auto opIdx = ctx.def.Size();
    CJC_ASSERT(opIdx <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
    PushOpCodeWithAnnotations<false>(ctx, OpCode::BOX, expr, 0);
    auto ty = expr.GetObject()->GetType();
    if (ty->IsStruct()) {
        auto structTy = StaticCast<const StructType*>(ty);
        auto structDef = structTy->GetStructDef();
        ctx.def.AddMangledNameAnnotation(
            static_cast<Bchir::ByteCodeIndex>(opIdx), structDef->GetIdentifierWithoutPrefix());
    } else if (ty->IsEnum()) {
        auto enumTy = StaticCast<const EnumType*>(ty);
        auto enumDef = enumTy->GetEnumDef();
        ctx.def.AddMangledNameAnnotation(
            static_cast<Bchir::ByteCodeIndex>(opIdx), enumDef->GetIdentifierWithoutPrefix());
    } else { // this is a primitive type
        CJC_ASSERT(ty->IsPrimitive());
        ctx.def.AddMangledNameAnnotation(static_cast<Bchir::ByteCodeIndex>(opIdx), ty->ToString());
    }
}

void CHIR2BCHIR::TranslateCApplyExpression(Context& ctx, const Apply& apply, const Cangjie::CHIR::FuncType& funcTy)
{
    // bchir :: CAPPLY
    // :: CFUNC_NUMBER_OF_ARGS :: CFUNC_RESULT_TY :: CFUNC_ARG1_TY :: ... :: CFUNC_ARGN_TY
    // The number of funcTy and GetNumOfOperands
    size_t numberArgs = apply.GetNumOfOperands() - 1; // remove param 0 func;
    CJC_ASSERT(numberArgs == funcTy.GetParamTypes().size());
    CJC_ASSERT(numberArgs <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
    PushOpCodeWithAnnotations<false, true>(
        ctx, OpCode::CAPPLY, apply, static_cast<unsigned>(funcTy.GetParamTypes().size()));
    auto addTyAnnotation = [this, &ctx](CHIR::Type& type) { ctx.def.Push(GetTypeIdx(type)); };
    addTyAnnotation(*funcTy.GetReturnType());
    for (auto ty : funcTy.GetParamTypes()) {
        addTyAnnotation(*ty);
    }
}

void CHIR2BCHIR::TranslateApplyExpression(Context& ctx, const Apply& apply)
{
    auto operands = apply.GetOperands();
    auto funcExpr = operands[0];
    auto funcTy = funcExpr->GetType();
    if ((funcExpr->IsImportedFunc() && funcExpr->GetAttributeInfo().TestAttr(Attribute::FOREIGN)) ||
        funcExpr->GetSrcCodeIdentifier() == "std.core:CJ_CORE_CanUseSIMD") {
        // This is an hack. These functions should be intrinsic in CHIR 2.0. For the time being
        // we simply translate them as INTRINSIC1.
        auto it = syscall2IntrinsicKind.find(funcExpr->GetSrcCodeIdentifier());
        if (it != syscall2IntrinsicKind.end()) {
            // We use INTRINSIC1 instead of INTRINSIC0 so that we know that the dummy function node
            // needs to be popped from the argument stack. Revert changes once these functions are marked
            // as intrinsic in CHIR 2.0.
            PushOpCodeWithAnnotations<false, true>(ctx, OpCode::INTRINSIC1, apply, it->second, UINT32_MAX);
            return;
        }
        // bchir :: SYSCALL :: syscallName_STRING_IDX :: NUMBER_OF_ARGS
        // :: ANNOTATION_RESULT_TY :: ANNOTATION_ARG1_TY :: ... :: ANNOTATION_ARGN_TY
        auto strIdx = GetStringIdx(funcExpr->GetSrcCodeIdentifier());
        auto numberArgs = apply.GetNumOfOperands() - 1;
        CJC_ASSERT(numberArgs <= static_cast<size_t>(Bchir::BYTECODE_CONTENT_MAX));
        PushOpCodeWithAnnotations<false, true>(
            ctx, OpCode::SYSCALL, apply, strIdx, static_cast<Bchir::ByteCodeContent>(numberArgs));
        auto addTyAnnotation = [this, &ctx](CHIR::Type& type) { ctx.def.Push(GetTypeIdx(type)); };
        addTyAnnotation(*apply.GetResult()->GetType());
        // skip the first operand which is the function
        for (size_t i = 1; i < operands.size(); ++i) {
            addTyAnnotation(*operands[i]->GetType());
        }
        return;
    } else if (StaticCast<const FuncType&>(*funcTy).IsCFunc()) {
        TranslateCApplyExpression(ctx, apply, StaticCast<const FuncType&>(*funcTy));
        return;
    }
    PushOpCodeWithAnnotations<false, true>(ctx, OpCode::APPLY, apply, static_cast<unsigned>(apply.GetNumOfOperands()));
}

void CHIR2BCHIR::TranslateVArrayBuilder(Context& ctx, const VArrayBuilder& vArrayBuilder)
{
    // introduces bytecode to initialize the VArray
    // :: LVAR :: ID1 :: FIELD :: 1 :: LVAR :: ID1 :: FIELD :: 0 :: INT64 :: 0 :: APPLY ::
    // ... :: LVAR :: ID1 :: FIELD :: 1 :: LVAR :: ID1 :: FIELD :: 1 :: INT64 :: n
    // where ID1 is the local variable ID asociated to vArrayBuilder.GetOperands()[2] and
    // n is the size of the array - 1
    const size_t CLOSURE_IDX = 2;
    CJC_ASSERT(vArrayBuilder.GetOperands()[0]->IsLocalVar());
    CJC_ASSERT(vArrayBuilder.GetOperands()[1]->IsLocalVar());
    CJC_ASSERT(vArrayBuilder.GetOperands()[CLOSURE_IDX]->IsLocalVar());
    auto vArraySizeVar = StaticCast<const LocalVar*>(vArrayBuilder.GetOperands()[0]);
    CJC_ASSERT(vArraySizeVar->GetExpr()->IsConstant());
    auto vArrayConstant = StaticCast<const Constant*>(vArraySizeVar->GetExpr());
    auto vArraySizeLit = StaticCast<const IntLiteral*>(vArrayConstant->GetValue());
    auto vArraySize = vArraySizeLit->GetSignedVal();
    auto initVar = StaticCast<const LocalVar*>(vArrayBuilder.GetOperands()[1]);
    auto closureVar = vArrayBuilder.GetOperands()[CLOSURE_IDX];
    CJC_ASSERT(vArraySize >= 0);
    if (initVar->GetExpr()->IsConstantNull()) {
        GenerateVArrayInitializer(ctx, vArrayBuilder, static_cast<uint64_t>(vArraySize), *closureVar);
    } else {
        PushOpCodeWithAnnotations(ctx, OpCode::VARRAY_BY_VALUE, vArrayBuilder);
    }
}

void CHIR2BCHIR::GenerateVArrayInitializer(
    Context& ctx, const VArrayBuilder& vArrayBuilder, uint64_t vArraySize, const CHIR::Value& closure)
{
    const Bchir::ByteCodeContent NUMBER_ARGS = 3;
    PushOpCodeWithAnnotations(ctx, OpCode::DROP, vArrayBuilder); // pop closure from stack
    PushOpCodeWithAnnotations(ctx, OpCode::DROP, vArrayBuilder); // pop nullptr from argStack
    PushOpCodeWithAnnotations(ctx, OpCode::DROP, vArrayBuilder); // pop size from argStack

    // ALLOCATE :: LVAR_SET :: indexVarId
    PushOpCodeWithAnnotations(ctx, OpCode::ALLOCATE, vArrayBuilder);
    auto indexVarId = ctx.localVarId++;
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR_SET, vArrayBuilder, indexVarId);

    // INT64 :: 0 :: LVAR :: indexVarId :: STORE
    PushOpCodeWithAnnotations(ctx, OpCode::INT64, vArrayBuilder);
    ctx.def.Push8bytes(static_cast<uint64_t>(0));
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, indexVarId);
    PushOpCodeWithAnnotations(ctx, OpCode::STORE, vArrayBuilder);

    // LVAR :: indexVarId :: DEREF :: INT64 :: vArraySize :: BIN_LT :: BRANCH :: i1 :: i2
    auto loopBegin = static_cast<Bchir::ByteCodeIndex>(ctx.def.Size());
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, indexVarId);
    PushOpCodeWithAnnotations(ctx, OpCode::DEREF, vArrayBuilder);
    PushOpCodeWithAnnotations(ctx, OpCode::INT64, vArrayBuilder);
    ctx.def.Push8bytes(vArraySize);
    auto typeKind = static_cast<Bchir::ByteCodeContent>(CHIR::Type::TypeKind::TYPE_INT64);
    auto overflowStrat = static_cast<Bchir::ByteCodeContent>(Cangjie::OverflowStrategy::NA);
    PushOpCodeWithAnnotations(ctx, OpCode::BIN_LT, vArrayBuilder, typeKind, overflowStrat);
    auto branchIdx = static_cast<Bchir::ByteCodeIndex>(ctx.def.Size());
    PushOpCodeWithAnnotations(
        ctx, OpCode::BRANCH, vArrayBuilder, branchIdx + 1, static_cast<Bchir::ByteCodeContent>(0));

    // LVAR :: LVarId(closure) :: FIELD :: 1 :: LVAR :: LVarId(closure) :: FIELD :: 0 ::
    // LVAR :: indexVarId :: DEREF :: APPLY :: 3
#if CANGJIE_CODEGEN_CJNATIVE_BACKEND
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, LVarId(ctx, closure));
    PushOpCodeWithAnnotations(ctx, OpCode::FIELD, vArrayBuilder, 0);
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, LVarId(ctx, closure));
    PushOpCodeWithAnnotations(ctx, OpCode::FIELD, vArrayBuilder, 1);
#else
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, LVarId(ctx, closure));
    PushOpCodeWithAnnotations(ctx, OpCode::FIELD, vArrayBuilder, 1);
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, LVarId(ctx, closure));
    PushOpCodeWithAnnotations(ctx, OpCode::FIELD, vArrayBuilder, 0);
#endif
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, indexVarId);
    PushOpCodeWithAnnotations(ctx, OpCode::DEREF, vArrayBuilder);
    PushOpCodeWithAnnotations(ctx, OpCode::APPLY, vArrayBuilder, NUMBER_ARGS);

    // VAR :: indexVarId :: DEREF :: INT64 :: 1 :: BIN_ADD ::
    // LVAR :: STORE :: JUMP :: loopBegin
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, indexVarId);
    PushOpCodeWithAnnotations(ctx, OpCode::DEREF, vArrayBuilder);
    PushOpCodeWithAnnotations(ctx, OpCode::INT64, vArrayBuilder);
    ctx.def.Push8bytes(static_cast<uint64_t>(1));
    PushOpCodeWithAnnotations(ctx, OpCode::BIN_ADD, vArrayBuilder, typeKind, overflowStrat);
    PushOpCodeWithAnnotations(ctx, OpCode::LVAR, vArrayBuilder, indexVarId);
    PushOpCodeWithAnnotations(ctx, OpCode::STORE, vArrayBuilder);
    PushOpCodeWithAnnotations(ctx, OpCode::JUMP, vArrayBuilder, loopBegin);

    // set indexes
    const Bchir::ByteCodeContent OFFSET_TWO = 2;
    const Bchir::ByteCodeContent OFFSET_THREE = 3;
    auto afterLoop = static_cast<Bchir::ByteCodeIndex>(ctx.def.Size());
    ctx.def.Set(branchIdx + 1, branchIdx + OFFSET_THREE);
    ctx.def.Set(branchIdx + OFFSET_TWO, afterLoop);

    // VARRAY :: vArraySize
    PushOpCodeWithAnnotations(ctx, OpCode::VARRAY, vArrayBuilder, static_cast<unsigned>(vArraySize));
}
