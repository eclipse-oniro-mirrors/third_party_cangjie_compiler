// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements a translation from CHIR intrinsics to BCHIR intrinsics.
 */
#include "cangjie/CHIR/Interpreter/BCHIR.h"
#include "cangjie/CHIR/Interpreter/CHIR2BCHIR.h"
#include "cangjie/CHIR/Interpreter/Utils.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Type/StructDef.h"

using namespace Cangjie::CHIR;
using namespace Interpreter;

template <typename T> void CHIR2BCHIR::TranslateIntrinsicExpression(Context& ctx, const T& intrinsic)
{
    /* One of the following depending on the necessary additional information
        bchir :: INTRINSIC0 :: INTRINSIC_KIND
    or
        bchir :: INTRINSIC0 :: INTRINSIC_KIND :: AUX_INFO1
    or
        bchir :: INTRINSIC2 :: INTRINSIC_KIND :: AUX_INFO2 :: AUX_INFO2

    Note that we insert an additional argument to store additional information. For some intrinsic
    functions we need to store some type information. Depending on the intrinsic operation this can
    represent different things. */

    if (intrinsic.GetIntrinsicKind() == CHIR::IntrinsicKind::CG_UNSAFE_BEGIN ||
        intrinsic.GetIntrinsicKind() == CHIR::IntrinsicKind::CG_UNSAFE_END) {
        return;
    }

    auto isCType = [](const CHIR::Type& ty) {
        // T0D0: is an array of arrays a C type? probably yes!
        return ty.IsPrimitive() || ty.IsCString() ||
            (ty.IsStruct() && StaticCast<const StructType*>(&ty)->GetStructDef()->IsCStruct());
    };

    std::vector<Bchir::ByteCodeContent> auxInfo{};
    switch (intrinsic.GetIntrinsicKind()) {
        case CHIR::IntrinsicKind::ARRAY_BUILT_IN_COPY_TO:
        case CHIR::IntrinsicKind::ARRAY_CLONE:
        case CHIR::IntrinsicKind::ARRAY_ACQUIRE_RAW_DATA: {
            auto refTy = StaticCast<RefType*>(intrinsic.GetOperands()[0]->GetType());
            auto arrayTy = StaticCast<RawArrayType*>(refTy->GetTypeArgs()[0]);
            auto valueTy = arrayTy->GetTypeArgs()[0];
            if (isCType(*valueTy)) {
                // T0D0: can we store the type of the array content instead?
                auxInfo.emplace_back(GetTypeIdx(*arrayTy));
            } else {
                auxInfo.emplace_back(UINT32_MAX);
            }
            break;
        }
        case CHIR::IntrinsicKind::CPOINTER_ADD: {
            auto cPointerTy = static_cast<CPointerType*>(intrinsic.GetOperands()[0]->GetType());
            auto contentTy = cPointerTy->GetElementType();
            auxInfo.emplace_back(GetTypeIdx(*contentTy));
            break;
        }
        case CHIR::IntrinsicKind::CPOINTER_WRITE: {
            auto cPointerTy = static_cast<CPointerType*>(intrinsic.GetOperands()[2]->GetType());
            auxInfo.emplace_back(GetTypeIdx(*cPointerTy));
            break;
        }
        case CHIR::IntrinsicKind::CPOINTER_READ: {
            auto valueTy = static_cast<CPointerType*>(intrinsic.GetResult()->GetType());
            auxInfo.emplace_back(GetTypeIdx(*valueTy));
            break;
        }
        case CHIR::IntrinsicKind::ARRAY_GET:
        case CHIR::IntrinsicKind::ARRAY_GET_UNCHECKED:
        case CHIR::IntrinsicKind::ARRAY_SET:
        case CHIR::IntrinsicKind::ARRAY_SET_UNCHECKED: {
            auto refTy = StaticCast<RefType*>(intrinsic.GetOperands()[0]->GetType());
            auto arrayTy = StaticCast<RawArrayType*>(refTy->GetTypeArgs()[0]);
            auto valueTy = arrayTy->GetTypeArgs()[0];
            if (isCType(*valueTy)) {
                auxInfo.emplace_back(GetTypeIdx(*valueTy));
            } else {
                auxInfo.emplace_back(UINT32_MAX);
            }
            break;
        }
        case CHIR::IntrinsicKind::VARRAY_GET: {
            auto pathSize = static_cast<Bchir::ByteCodeContent>(intrinsic.GetNumOfOperands());
            PushOpCodeWithAnnotations<false, true>(ctx, OpCode::VARRAY_GET, intrinsic, pathSize - 1);
            return;
        }
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case CHIR::IntrinsicKind::ATOMIC_LOAD: {
            return TranslateAtomicLoad(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_STORE: {
            return TranslateAtomicStore(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_SWAP: {
            return TranslateAtomicSwap(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_COMPARE_AND_SWAP: {
            return TranslateAtomicCAS(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_FETCH_ADD: {
            return TranslateAtomicFetchAdd(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_FETCH_SUB: {
            return TranslateAtomicFetchSub(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_FETCH_AND: {
            return TranslateAtomicFetchAnd(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_FETCH_OR: {
            return TranslateAtomicFetchOr(ctx, intrinsic);
        }
        case CHIR::IntrinsicKind::ATOMIC_FETCH_XOR: {
            return TranslateAtomicFetchXor(ctx, intrinsic);
        }
#endif // CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case CHIR::IntrinsicKind::BEGIN_CATCH:
            // Behaves like id function.
            return;
        case CHIR::IntrinsicKind::GET_TYPE_FOR_TYPE_PARAMETER:
        case CHIR::IntrinsicKind::ALIGN_OF:
        case CHIR::IntrinsicKind::SIZE_OF: {
            // OPTIMIZE: no need for a call, just return the result
            auto instTypes = intrinsic.GetGenericTypeInfo();
            CJC_ASSERT(instTypes.size() == 1);
            auxInfo.emplace_back(GetTypeIdx(*instTypes[0]));
            break;
        }
        case CHIR::IntrinsicKind::CPOINTER_INIT1:
        case CHIR::IntrinsicKind::CPOINTER_INIT0:
        case CHIR::IntrinsicKind::OBJECT_REFEQ:
        case CHIR::IntrinsicKind::CPOINTER_GET_POINTER_ADDRESS:
        case CHIR::IntrinsicKind::OBJECT_ZERO_VALUE:
        case CHIR::IntrinsicKind::ARRAY_RELEASE_RAW_DATA:
        case CHIR::IntrinsicKind::FILL_IN_STACK_TRACE:
        case CHIR::IntrinsicKind::CSTRING_INIT:
        case CHIR::IntrinsicKind::GET_THREAD_OBJECT:
        case CHIR::IntrinsicKind::SET_THREAD_OBJECT:
        case CHIR::IntrinsicKind::DECODE_STACK_TRACE:
        case CHIR::IntrinsicKind::CSTRING_CONVERT_CSTR_TO_PTR:
        case CHIR::IntrinsicKind::ARRAY_SIZE:
        case CHIR::IntrinsicKind::SLEEP: {
            break; // nothing to do here, just trying to be exhaustive
        }
        // new intrinsic functions defined in std/math/native_llvmgc.cj and not yet supported by the interpreter
        case CHIR::IntrinsicKind::SIN:
        case CHIR::IntrinsicKind::COS:
        case CHIR::IntrinsicKind::EXP:
        case CHIR::IntrinsicKind::EXP2:
        case CHIR::IntrinsicKind::LOG:
        case CHIR::IntrinsicKind::LOG2:
        case CHIR::IntrinsicKind::LOG10:
        case CHIR::IntrinsicKind::SQRT:
        case CHIR::IntrinsicKind::FLOOR:
        case CHIR::IntrinsicKind::CEIL:
        case CHIR::IntrinsicKind::TRUNC:
        case CHIR::IntrinsicKind::ROUND:
        case CHIR::IntrinsicKind::FABS:
        case CHIR::IntrinsicKind::ABS:
        case CHIR::IntrinsicKind::POW:
        case CHIR::IntrinsicKind::POWI: {
            break;
        }
        // concurrency primitives not supported in the interpreter - std/sync/native.cj
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case CHIR::IntrinsicKind::MUTEX_INIT:
        case CHIR::IntrinsicKind::CJ_MUTEX_LOCK:
        case CHIR::IntrinsicKind::MUTEX_TRY_LOCK:
        case CHIR::IntrinsicKind::MUTEX_CHECK_STATUS:
        case CHIR::IntrinsicKind::MUTEX_UNLOCK:
        case CHIR::IntrinsicKind::MOITIOR_WAIT:
        case CHIR::IntrinsicKind::MOITIOR_NOTIFY:
        case CHIR::IntrinsicKind::MOITIOR_NOTIFY_ALL:
        case CHIR::IntrinsicKind::MULTICONDITION_WAIT:
        case CHIR::IntrinsicKind::MULTICONDITION_NOTIFY:
        case CHIR::IntrinsicKind::MULTICONDITION_NOTIFY_ALL:
        case CHIR::IntrinsicKind::WAITQUEUE_INIT:
        case CHIR::IntrinsicKind::MONITOR_INIT:
        // concurrency primitives not supported in the interpreter - std/core/future.cj
        case CHIR::IntrinsicKind::FUTURE_INIT:
        case CHIR::IntrinsicKind::FUTURE_IS_COMPLETE:
        case CHIR::IntrinsicKind::FUTURE_WAIT:
        case CHIR::IntrinsicKind::FUTURE_NOTIFYALL: {
            break;
        }
#endif // CANGJIE_CODEGEN_CJNATIVE_BACKEND
        /* we just translate intrinsic overflow operations into normal arithmetic operations with appropriate overflow
         * strategy */
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_ADD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_ADD, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_SUB:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_SUB, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_MUL:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MUL, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_DIV:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_DIV, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_MOD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MOD, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_POW:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_EXP, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_INC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_INC, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_DEC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_DEC, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_CHECKED_NEG:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_NEG, OverflowStrategy::CHECKED>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_ADD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_ADD, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_SUB:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_SUB, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_MUL:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MUL, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_DIV:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_DIV, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_MOD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MOD, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_POW:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_EXP, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_INC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_INC, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_DEC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_DEC, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_THROWING_NEG:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_NEG, OverflowStrategy::THROWING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_ADD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_ADD, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_SUB:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_SUB, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_MUL:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MUL, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_DIV:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_DIV, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_MOD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MOD, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_POW:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_EXP, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_INC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_INC, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_DEC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_DEC, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_SATURATING_NEG:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_NEG, OverflowStrategy::SATURATING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_ADD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_ADD, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_SUB:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_SUB, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_MUL:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MUL, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_DIV:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_DIV, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_MOD:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_MOD, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_POW:
            return TranslateOpsWithOverflowStrat<T, OpCode::BIN_EXP, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_INC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_INC, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_DEC:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_DEC, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::OVERFLOW_WRAPPING_NEG:
            return TranslateOpsWithOverflowStrat<T, OpCode::UN_NEG, OverflowStrategy::WRAPPING>(ctx, intrinsic);
        case CHIR::IntrinsicKind::ARRAY_SLICE_GET_ELEMENT:
            [[fallthrough]];
        case CHIR::IntrinsicKind::ARRAY_SLICE_GET_ELEMENT_UNCHECKED:
            [[fallthrough]];
        case CHIR::IntrinsicKind::ARRAY_SLICE_SET_ELEMENT:
            [[fallthrough]];
        case CHIR::IntrinsicKind::ARRAY_SLICE_SET_ELEMENT_UNCHECKED: {
            // these operations need to encode the overflow strategy of the intrinsic syscall
            auto structTy = StaticCast<const StructType*>(intrinsic.GetOperands()[0]->GetType());
            auto valueTy = structTy->GetTypeArgs()[0];
            if (isCType(*valueTy)) {
                auxInfo.emplace_back(GetTypeIdx(*valueTy));
            } else {
                auxInfo.emplace_back(UINT32_MAX);
            }
            auto overflowStrat = OverflowStrategy::WRAPPING;
            auxInfo.emplace_back(static_cast<Bchir::ByteCodeContent>(overflowStrat));
            break;
        }
        default: {
            // by default translate unlisted intrinsic as INTRINSIC0
            break;
        }
    }
    auto intrinsicKind = static_cast<Bchir::ByteCodeContent>(intrinsic.GetIntrinsicKind());
    CJC_ASSERT(auxInfo.size() <= Bchir::FLAG_THREE);
    auto opCode =
        auxInfo.size() == 0 ? OpCode::INTRINSIC0 : (auxInfo.size() == 1 ? OpCode::INTRINSIC1 : OpCode::INTRINSIC2);
    if (intrinsic.GetExprKind() != ExprKind::INTRINSIC) {
        CJC_ASSERT(intrinsic.GetExprKind() == ExprKind::INTRINSIC_WITH_EXCEPTION);
        opCode = static_cast<OpCode>(static_cast<size_t>(opCode) + Bchir::FLAG_THREE);
    }
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, intrinsicKind);
    if (auxInfo.size() > 0) {
        ctx.def.Push(auxInfo[0]);
        if (auxInfo.size() > 1) {
            ctx.def.Push(auxInfo[1]);
        }
    }
}

template <typename T, OpCode O, Cangjie::OverflowStrategy S>
void CHIR2BCHIR::TranslateOpsWithOverflowStrat(Context& ctx, const T& intrinsic)
{
    // O :: TYPE :: S
    auto argTy = intrinsic.GetOperands()[0]->GetType();
    CJC_ASSERT(argTy != nullptr);
    auto kind = static_cast<Bchir::ByteCodeContent>(argTy->GetTypeKind());
    CJC_ASSERT(kind >= static_cast<size_t>(CHIR::Type::TypeKind::TYPE_INT8) &&
        kind <= static_cast<size_t>(CHIR::Type::TypeKind::TYPE_UINT_NATIVE));
    PushOpCodeWithAnnotations<false, true>(ctx, O, intrinsic, kind, static_cast<Bchir::ByteCodeContent>(S));
}

// force instantiation of TranslateIntrinsicExpression with Intrinsic and IntrinsicWithException
template void CHIR2BCHIR::TranslateIntrinsicExpression(Context& ctx, const Intrinsic& intrinsic);
template void CHIR2BCHIR::TranslateIntrinsicExpression(Context& ctx, const IntrinsicWithException& intrinsic);

template <typename T> void CHIR2BCHIR::TranslateAtomicStore(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetOperands()[1]->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_REFTYPE:
            kind = CHIR::IntrinsicKind::ATOMIC_REFERENCEBASE_STORE;
            break;
        case CHIR::Type::TypeKind::TYPE_ENUM:
            kind = CHIR::IntrinsicKind::ATOMIC_OPTIONREFERENCE_STORE;
            break;
        default:
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicLoad(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_REFTYPE:
            kind = CHIR::IntrinsicKind::ATOMIC_REFERENCEBASE_LOAD;
            break;
        case CHIR::Type::TypeKind::TYPE_ENUM:
            kind = CHIR::IntrinsicKind::ATOMIC_OPTIONREFERENCE_LOAD;
            break;
        default:
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicCAS(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetOperands()[1]->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_REFTYPE:
            kind = CHIR::IntrinsicKind::ATOMIC_REFERENCEBASE_CAS;
            break;
        case CHIR::Type::TypeKind::TYPE_ENUM:
            kind = CHIR::IntrinsicKind::ATOMIC_OPTIONREFERENCE_CAS;
            break;
        default:
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicFetchAdd(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_FETCH_ADD;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_FETCH_ADD;
            break;
        default:
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicSwap(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetOperands()[1]->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_REFTYPE:
            kind = CHIR::IntrinsicKind::ATOMIC_REFERENCEBASE_SWAP;
            break;
        case CHIR::Type::TypeKind::TYPE_ENUM:
            kind = CHIR::IntrinsicKind::ATOMIC_OPTIONREFERENCE_SWAP;
            break;
        default:
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicFetchSub(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TypeKind::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_FETCH_SUB;
            break;
        case CHIR::Type::TypeKind::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_FETCH_SUB;
            break;
        default:
            CJC_ASSERT(false);
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicFetchAnd(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_FETCH_AND;
            break;
        case CHIR::Type::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_FETCH_AND;
            break;
        case CHIR::Type::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_FETCH_AND;
            break;
        case CHIR::Type::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_FETCH_AND;
            break;
        case CHIR::Type::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_FETCH_AND;
            break;
        case CHIR::Type::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_FETCH_AND;
            break;
        case CHIR::Type::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_FETCH_AND;
            break;
        case CHIR::Type::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_FETCH_AND;
            break;
        default:
            CJC_ASSERT(false);
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicFetchOr(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_FETCH_OR;
            break;
        case CHIR::Type::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_FETCH_OR;
            break;
        case CHIR::Type::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_FETCH_OR;
            break;
        case CHIR::Type::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_FETCH_OR;
            break;
        case CHIR::Type::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_FETCH_OR;
            break;
        case CHIR::Type::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_FETCH_OR;
            break;
        case CHIR::Type::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_FETCH_OR;
            break;
        case CHIR::Type::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_FETCH_OR;
            break;
        default:
            CJC_ASSERT(false);
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}

template <typename T> void CHIR2BCHIR::TranslateAtomicFetchXor(Context& ctx, const T& intrinsic)
{
    // T0D0: this should possibly be a simple INTRINSIC1 with the type as argument
    auto ty = intrinsic.GetResult()->GetType();
    auto kind = intrinsic.GetIntrinsicKind();
    switch (ty->GetTypeKind()) {
        case CHIR::Type::TYPE_INT8:
            kind = CHIR::IntrinsicKind::ATOMIC_INT8_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_INT16:
            kind = CHIR::IntrinsicKind::ATOMIC_INT16_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_INT32:
            kind = CHIR::IntrinsicKind::ATOMIC_INT32_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_INT64:
            kind = CHIR::IntrinsicKind::ATOMIC_INT64_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_UINT8:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT8_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_UINT16:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT16_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_UINT32:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT32_FETCH_XOR;
            break;
        case CHIR::Type::TYPE_UINT64:
            kind = CHIR::IntrinsicKind::ATOMIC_UINT64_FETCH_XOR;
            break;
        default:
            CJC_ASSERT(false);
            CJC_ABORT();
    }
    auto opCode = intrinsic.GetExprKind() == ExprKind::INTRINSIC ? OpCode::INTRINSIC0 : OpCode::INTRINSIC0_EXC;
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, intrinsic, static_cast<Bchir::ByteCodeContent>(kind));
}