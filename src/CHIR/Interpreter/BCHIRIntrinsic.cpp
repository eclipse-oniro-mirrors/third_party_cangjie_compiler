// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements intrinsics functions in the interpreter for the standard library.
 */

#include <securec.h>
#include <thread>

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Interpreter/BCHIRInterpreter.h"
#include "cangjie/CHIR/IntrinsicKind.h"
#include "cangjie/Utils/ConstantsUtils.h"

using namespace Cangjie;
using namespace CHIR;
using namespace Interpreter;

namespace IntrinsicsCHIR {
const static size_t UNIT_LEN = 2; // supports "kb", "mb", "gb".
const static int64_t KB = 1024;
const static int64_t MB = KB * KB;
const static int64_t GB = MB * MB;
const int64_t DEFAULT_HEAP_SIZE = 64 * MB;

int64_t GetSizeFromEnv(std::string& str)
{
    (void)str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    size_t len = str.length();
    // The last two characters are units, such as "kb".
    if (len <= UNIT_LEN) {
        return 0;
    }
    // Split size and unit.
    auto num = str.substr(0, len - UNIT_LEN);
    auto unit = str.substr(len - UNIT_LEN, UNIT_LEN);

    int64_t iRes = std::stoi(num);
    if (iRes <= 0) {
        return 0;
    }
    (void)std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);
    if (unit == "kb") {
        return iRes * KB;
    } else if (unit == "mb") {
        // unit: 1024 * 1KB = 1MB
        return iRes * MB;
    } else if (unit == "gb") {
        // unit: 1024 * 1024 * 1KB = 1GB
        return iRes * GB;
    }
    return 0;
}

/**
 * Get heap size from environment variable.
 * The unit must be added when configuring "cjHeapSize", it supports "kb", "mb", "gb".
 * for example:
 *     export cjHeapSize = 16GB
 */
int64_t GetHeapSizeFromEnv()
{
    const char* str = std::getenv("cjHeapSize");
    if (str == nullptr) {
        return DEFAULT_HEAP_SIZE;
    }
    std::string sizeStr(str);
    auto heapSize = GetSizeFromEnv(sizeStr);
    if (heapSize == 0) {
        return DEFAULT_HEAP_SIZE;
    }
    return heapSize;
}
} // namespace IntrinsicsCHIR

bool BCHIRInterpreter::InterpretIntrinsic0()
{
    CJC_ASSERT(static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC0 ||
        static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC0_EXC);
    // INTRINSIC :: INTRINSIC_KIND
    auto opIdx = pc++;
    auto intrinsicKind = static_cast<IntrinsicKind>(bchir.Get(pc++));
    switch (intrinsicKind) {
        case STRLEN:
            InterpretCRTSTRLEN();
            return false;
        case MEMCPY_S:
            InterpretCRTMEMCPYS();
            return false;
        case MEMSET_S:
            InterpretCRTMEMSETS();
            return false;
        case FREE:
            InterpretCRTFREE();
            return false;
        case MALLOC:
            InterpretCRTMALLOC();
            return false;
        case STRCMP:
            InterpretCRTSTRCMP();
            return false;
        case MEMCMP:
            InterpretCRTMEMCMP();
            return false;
        case STRNCMP:
            InterpretCRTSTRNCMP();
            return false;
        case STRCASECMP:
            InterpretCRTSTRCASECMP();
            return false;
        case VARRAY_SET:
            return InterpretVArraySetIntrinsic(opIdx, true);
        case ARRAY_SIZE:
            InterpretArraySizeIntrinsic();
            return false;
        case OBJECT_ZERO_VALUE:
            InterpretObjectZeroValue();
            return false;
        case CPOINTER_INIT0:
            InterpretCPointerInitIntrinsic(false);
            return false;
        case CPOINTER_INIT1:
            InterpretCPointerInitIntrinsic(true);
            return false;
        case CPOINTER_GET_POINTER_ADDRESS:
            InterpretCPointerGetPointerAddressIntrinsic();
            return false;
        case ARRAY_INIT:
            InterpretArrayInitIntrinsic();
            return false;
        case ARRAY_SLICE_INIT:
            InterpretArraySliceInitIntrinsic();
            return false;
        case ARRAY_SLICE_RAWARRAY:
            InterpretArraySliceRawArrayIntrinsic();
            return false;
        case ARRAY_SLICE_START:
            InterpretArraySliceStartIntrinsic();
            return false;
        case ARRAY_SLICE_SIZE:
            InterpretArraySliceSizeIntrinsic();
            return false;
        case CHR:
            InterpretCHR();
            return false;
        case ORD:
            InterpretORD();
            return false;
        case SLEEP:
            InterpretSleep();
            return false;
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case RAW_ARRAY_REFEQ:
            [[fallthrough]];
#endif
        case OBJECT_REFEQ:
            InterpretRefEq();
            return false;
        case CSTRING_INIT:
            InterpretCStringInit();
            return false;
        case CSTRING_CONVERT_CSTR_TO_PTR:
            InterpretconvertCStr2Ptr();
            return false;
        case IDENTITY_HASHCODE:
            InterpretIdentityHashCode();
            return false;
        case IDENTITY_HASHCODE_FOR_ARRAY:
            InterpretIdentityHashCodeForArray();
            return false;
        case INVOKE_GC:
            InterpretInvokeGC();
            return false;
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case MONITOR_INIT:
        case MUTEX_INIT:
            PopsOneReturnsFalseDummy();
            return false;
        case CJ_MUTEX_LOCK:
        case MUTEX_UNLOCK:
        case MOITIOR_NOTIFY:
        case MOITIOR_NOTIFY_ALL:
            PopsOneReturnsUnitDummy();
            return false;
        case MUTEX_CHECK_STATUS:
        case MUTEX_TRY_LOCK:
            PopsOneReturnsTrueDummy();
            return false;
        case MOITIOR_WAIT:
            PopsTwoReturnsTrueDummy();
            return false;
        case FILL_IN_STACK_TRACE:
            InterpretFillInStackTrace();
            return false;
        case DECODE_STACK_TRACE:
            InterpretDecodeStackTrace();
            return false;
        case PREINITIALIZE:
            interpStack.ArgsPush(IUnit{});
            return false;
#endif
        case ATOMIC_BOOL_STORE:
        case ATOMIC_INT8_STORE:
        case ATOMIC_INT16_STORE:
        case ATOMIC_INT32_STORE:
        case ATOMIC_INT64_STORE:
        case ATOMIC_UINT8_STORE:
        case ATOMIC_UINT16_STORE:
        case ATOMIC_UINT32_STORE:
        case ATOMIC_UINT64_STORE:
        case ATOMIC_REFERENCEBASE_STORE:
        case ATOMIC_OPTIONREFERENCE_STORE:
            InterpretAtomicStore();
            return false;
        case ATOMIC_BOOL_LOAD:
        case ATOMIC_INT8_LOAD:
        case ATOMIC_INT16_LOAD:
        case ATOMIC_INT32_LOAD:
        case ATOMIC_INT64_LOAD:
        case ATOMIC_UINT8_LOAD:
        case ATOMIC_UINT16_LOAD:
        case ATOMIC_UINT32_LOAD:
        case ATOMIC_UINT64_LOAD:
        case ATOMIC_REFERENCEBASE_LOAD:
        case ATOMIC_OPTIONREFERENCE_LOAD:
            InterpretAtomicLoad();
            return false;
        case ATOMIC_BOOL_SWAP:
        case ATOMIC_INT8_SWAP:
        case ATOMIC_INT16_SWAP:
        case ATOMIC_INT32_SWAP:
        case ATOMIC_INT64_SWAP:
        case ATOMIC_UINT8_SWAP:
        case ATOMIC_UINT16_SWAP:
        case ATOMIC_UINT32_SWAP:
        case ATOMIC_UINT64_SWAP:
        case ATOMIC_REFERENCEBASE_SWAP:
        case ATOMIC_OPTIONREFERENCE_SWAP:
            InterpretAtomicSwap();
            return false;
        case ATOMIC_BOOL_CAS:
        case ATOMIC_INT8_CAS:
            InterpretAtomicCAS<IInt8>();
            return false;
        case ATOMIC_INT16_CAS:
            InterpretAtomicCAS<IInt16>();
            return false;
        case ATOMIC_INT32_CAS:
            InterpretAtomicCAS<IInt32>();
            return false;
        case ATOMIC_INT64_CAS:
            InterpretAtomicCAS<IInt64>();
            return false;
        case ATOMIC_UINT8_CAS:
            InterpretAtomicCAS<IUInt8>();
            return false;
        case ATOMIC_UINT16_CAS:
            InterpretAtomicCAS<IUInt16>();
            return false;
        case ATOMIC_UINT32_CAS:
            InterpretAtomicCAS<IUInt32>();
            return false;
        case ATOMIC_UINT64_CAS:
            InterpretAtomicCAS<IUInt64>();
            return false;
        case ATOMIC_REFERENCEBASE_CAS:
            InterpretAtomicCAS<IPointer>();
            return false;
        case ATOMIC_OPTIONREFERENCE_CAS:
            InterpretAtomicCAS<ITuple>();
            return false;
        case ATOMIC_INT8_FETCH_ADD:
            InterpretAtomicFetchAdd<IInt8, int8_t>();
            return false;
        case ATOMIC_INT16_FETCH_ADD:
            InterpretAtomicFetchAdd<IInt16, int16_t>();
            return false;
        case ATOMIC_INT32_FETCH_ADD:
            InterpretAtomicFetchAdd<IInt32, int32_t>();
            return false;
        case ATOMIC_INT64_FETCH_ADD:
            InterpretAtomicFetchAdd<IInt64, int64_t>();
            return false;
        case ATOMIC_UINT8_FETCH_ADD:
            InterpretAtomicFetchAdd<IUInt8, uint8_t>();
            return false;
        case ATOMIC_UINT16_FETCH_ADD:
            InterpretAtomicFetchAdd<IUInt16, uint16_t>();
            return false;
        case ATOMIC_UINT32_FETCH_ADD:
            InterpretAtomicFetchAdd<IUInt32, uint32_t>();
            return false;
        case ATOMIC_UINT64_FETCH_ADD:
            InterpretAtomicFetchAdd<IUInt64, uint64_t>();
            return false;
        case ATOMIC_INT8_FETCH_SUB:
            InterpretAtomicFetchSub<IInt8, int8_t>();
            return false;
        case ATOMIC_INT16_FETCH_SUB:
            InterpretAtomicFetchSub<IInt16, int16_t>();
            return false;
        case ATOMIC_INT32_FETCH_SUB:
            InterpretAtomicFetchSub<IInt32, int32_t>();
            return false;
        case ATOMIC_INT64_FETCH_SUB:
            InterpretAtomicFetchSub<IInt64, int64_t>();
            return false;
        case ATOMIC_UINT8_FETCH_SUB:
            InterpretAtomicFetchSub<IUInt8, uint8_t>();
            return false;
        case ATOMIC_UINT16_FETCH_SUB:
            InterpretAtomicFetchSub<IUInt16, uint16_t>();
            return false;
        case ATOMIC_UINT32_FETCH_SUB:
            InterpretAtomicFetchSub<IUInt32, uint32_t>();
            return false;
        case ATOMIC_UINT64_FETCH_SUB:
            InterpretAtomicFetchSub<IUInt64, uint64_t>();
            return false;
        case ATOMIC_INT8_FETCH_AND:
            InterpretAtomicFetchAnd<IInt8, int8_t>();
            return false;
        case ATOMIC_INT16_FETCH_AND:
            InterpretAtomicFetchAnd<IInt16, int16_t>();
            return false;
        case ATOMIC_INT32_FETCH_AND:
            InterpretAtomicFetchAnd<IInt32, int32_t>();
            return false;
        case ATOMIC_INT64_FETCH_AND:
            InterpretAtomicFetchAnd<IInt64, int64_t>();
            return false;
        case ATOMIC_UINT8_FETCH_AND:
            InterpretAtomicFetchAnd<IUInt8, uint8_t>();
            return false;
        case ATOMIC_UINT16_FETCH_AND:
            InterpretAtomicFetchAnd<IUInt16, uint16_t>();
            return false;
        case ATOMIC_UINT32_FETCH_AND:
            InterpretAtomicFetchAnd<IUInt32, uint32_t>();
            return false;
        case ATOMIC_UINT64_FETCH_AND:
            InterpretAtomicFetchAnd<IUInt64, uint64_t>();
            return false;
        case ATOMIC_INT8_FETCH_OR:
            InterpretAtomicFetchOr<IInt8, int8_t>();
            return false;
        case ATOMIC_INT16_FETCH_OR:
            InterpretAtomicFetchOr<IInt16, int16_t>();
            return false;
        case ATOMIC_INT32_FETCH_OR:
            InterpretAtomicFetchOr<IInt32, int32_t>();
            return false;
        case ATOMIC_INT64_FETCH_OR:
            InterpretAtomicFetchOr<IInt64, int64_t>();
            return false;
        case ATOMIC_UINT8_FETCH_OR:
            InterpretAtomicFetchOr<IUInt8, uint8_t>();
            return false;
        case ATOMIC_UINT16_FETCH_OR:
            InterpretAtomicFetchOr<IUInt16, uint16_t>();
            return false;
        case ATOMIC_UINT32_FETCH_OR:
            InterpretAtomicFetchOr<IUInt32, uint32_t>();
            return false;
        case ATOMIC_UINT64_FETCH_OR:
            InterpretAtomicFetchOr<IUInt64, uint64_t>();
            return false;
        case ATOMIC_INT8_FETCH_XOR:
            InterpretAtomicFetchXor<IInt8, int8_t>();
            return false;
        case ATOMIC_INT16_FETCH_XOR:
            InterpretAtomicFetchXor<IInt16, int16_t>();
            return false;
        case ATOMIC_INT32_FETCH_XOR:
            InterpretAtomicFetchXor<IInt32, int32_t>();
            return false;
        case ATOMIC_INT64_FETCH_XOR:
            InterpretAtomicFetchXor<IInt64, int64_t>();
            return false;
        case ATOMIC_UINT8_FETCH_XOR:
            InterpretAtomicFetchXor<IUInt8, uint8_t>();
            return false;
        case ATOMIC_UINT16_FETCH_XOR:
            InterpretAtomicFetchXor<IUInt16, uint16_t>();
            return false;
        case ATOMIC_UINT32_FETCH_XOR:
            InterpretAtomicFetchXor<IUInt32, uint32_t>();
            return false;
        case ATOMIC_UINT64_FETCH_XOR:
            InterpretAtomicFetchXor<IUInt64, uint64_t>();
            return false;
        case GET_MAX_HEAP_SIZE: {
            int64_t heapSize = IntrinsicsCHIR::GetHeapSizeFromEnv();
            interpStack.ArgsPush(IInt64{heapSize});
            return false;
        }
        case GET_ALLOCATE_HEAP_SIZE: {
            int64_t size = arena.GetAllocatedSize();
            interpStack.ArgsPush(IInt64{size});
            return false;
        }
        // This will become an FFI in later updates and
        // thus we just return a dummy value at the moment.
        case GET_REAL_HEAP_SIZE: {
            interpStack.ArgsPush(IInt64{0});
            return false;
        }
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case VECTOR_INDEX_BYTE_32: {
            InterpretVectorIndexByte32();
            return false;
        }
#endif
        case INOUT_PARAM:
            return false;
        case ARRAY_SLICE: {
            InterpretArraySliceIntrinsic();
            return false;
        }
        // defined in std/math/native_llvmgc.cj and not yet supported by the interpreter
        case SIN:
        case COS:
        case EXP:
        case EXP2:
        case LOG:
        case LOG2:
        case LOG10:
        case SQRT:
        case FLOOR:
        case CEIL:
        case TRUNC:
        case ROUND:
        case FABS:
        case ABS:
        case POW:
        case POWI:
        case REGISTER_WATCHED_OBJECT:
        case NOT_IMPLEMENTED:
        default: {
            auto intrinsic = static_cast<size_t>(intrinsicKind);
            std::string concurrencyKeyword =
                intrinsicKind >= GET_THREAD_OBJECT && intrinsicKind <= SET_THREAD_OBJECT ? "concurrency " : "";
            std::string errorMSg = "interpreter does not support " + concurrencyKeyword + "intrinsic function " +
                std::to_string(intrinsic);
            FailWith(opIdx, errorMSg, DiagKind::interp_unsupported, "InterpretIntrinsic",
                GetOpCodeLabel(OpCode::INTRINSIC0));
            return true;
        }
    }
}

bool BCHIRInterpreter::InterpretIntrinsic1()
{
    CJC_ASSERT(static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC1 ||
        static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC1_EXC);
    // INTRINSIC1 :: INTRINSIC_KIND :: AUX_INFO1
    auto opIdx = pc;
    // skip pc
    pc++;
    // get and skip intrinsic kind
    auto intrinsicKind = static_cast<IntrinsicKind>(bchir.Get(pc++));
    // skip auxiliary type info
    pc++;

    switch (intrinsicKind) {
        // There's no need for these functions to be INTRINSIC1 instead of INTRINSIC0.
        // We just mark them as INTRINSIC1 in CHIR2BCHIR to know that the dummy function argument
        // needs to be popped from the argument stack. Revert commit once the functions from
        // syscallIntrinsicMap are marked as intrinsic in CHIR.
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case CJ_CORE_CAN_USE_SIMD:
            InterpretCJCodeCanUseSIMD();
            return false;
#endif
        case CJ_TLS_DYN_SET_SESSION_CALLBACK:
            InterpretCJTLSDYNSetSessionCallback();
            return false;
        case CJ_TLS_DYN_SSL_INIT:
            InterpretCjTlsSslInit();
            return false;
        case ARRAY_BUILT_IN_COPY_TO:
            IntepretArrayCopyToIntrinsic(opIdx);
            return false;
        case GET_TYPE_FOR_TYPE_PARAMETER:
            InterpretGetTypeForTypeParameter(opIdx);
            return false;
        case ARRAY_CLONE:
            InterpretArrayCloneIntrinsic(opIdx);
            return false;
        case ARRAY_GET:
            return InterpretArrayGetIntrinsic(opIdx, true);
        case ARRAY_GET_UNCHECKED:
            return InterpretArrayGetIntrinsic(opIdx, false);
        case ARRAY_SET:
            return InterpretArraySetIntrinsic(opIdx, true);
        case ARRAY_SET_UNCHECKED:
            return InterpretArraySetIntrinsic(opIdx, false);
        default: {
            auto intrinsic = static_cast<size_t>(intrinsicKind);
            std::string concurrencyKeyword =
                intrinsicKind >= GET_THREAD_OBJECT && intrinsicKind <= SET_THREAD_OBJECT ? "concurrency " : "";
            std::string errorMSg = "interpreter does not support " + concurrencyKeyword + "intrinsic function " +
                std::to_string(intrinsic);
            FailWith(opIdx, errorMSg, DiagKind::interp_unsupported, "InterpretIntrinsic",
                GetOpCodeLabel(OpCode::INTRINSIC1));
            return true;
        }
    }
}

bool BCHIRInterpreter::InterpretIntrinsic2()
{
    CJC_ASSERT(static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC2 ||
        static_cast<OpCode>(bchir.Get(pc)) == OpCode::INTRINSIC2_EXC);
    // INTRINSIC2 :: INTRINSIC_KIND :: TYPE_INFO:: OVERFLOW_STRAT
    auto opIdx = pc;
    // skip pc
    pc++;
    // get and skip intrinsic kind
    auto intrinsicKind = static_cast<IntrinsicKind>(bchir.Get(pc++));
    // skip auxiliary type info
    pc++;

    switch (intrinsicKind) {
        case ARRAY_SLICE_GET_ELEMENT: {
            pc++;
            return InterpretArraySliceGetIntrinsic(opIdx, true);
        }
        case ARRAY_SLICE_GET_ELEMENT_UNCHECKED: {
            pc++;
            return InterpretArraySliceGetIntrinsic(opIdx, false);
        }
        case ARRAY_SLICE_SET_ELEMENT: {
            pc++;
            return InterpretArraySliceSetIntrinsic(opIdx, true);
        }
        case ARRAY_SLICE_SET_ELEMENT_UNCHECKED: {
            pc++;
            return InterpretArraySliceSetIntrinsic(opIdx, false);
        }
        default: {
            auto intrinsic = static_cast<size_t>(intrinsicKind);
            std::string errorMSg = "interpreter does not support intrinsic function " + std::to_string(intrinsic);
            FailWith(opIdx, errorMSg, DiagKind::interp_unsupported, "InterpretIntrinsic",
                GetOpCodeLabel(OpCode::INTRINSIC2));
            return false;
        }
    }
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
void BCHIRInterpreter::InterpretCJCodeCanUseSIMD()
{
    // this is an hack to remove the dummy func value
    interpStack.ArgsRemove(1);
    static int8_t simdSUPPORT = -1;
    if (simdSUPPORT < 0) {
#if defined(__linux__) || defined(__APPLE__)
#if defined(__x86_64__)
        __builtin_cpu_init();
        simdSUPPORT = __builtin_cpu_supports("avx") && __builtin_cpu_supports("avx2");
#elif defined(__aarch64__)
        simdSUPPORT = 1;
#else
        simdSUPPORT = 0;
#endif
#else
        simdSUPPORT = 0;
#endif
    }
    bool ret = (simdSUPPORT > 0);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(ret));
}
#endif

void BCHIRInterpreter::InterpretCJTLSDYNSetSessionCallback()
{
    // This is a dummy implementaion of foreign function CJ_TLS_DYN_SetSessionCallback from libs/net/tls/sessions.cj.
    // We need this dummy implementation because sometimes package net.tls is imported as macro package.
    // CJ_TLS_DYN_SetSessionCallback is called in CJ_TLS_SetSessionCallback and CJ_TLS_SetSessionCallback is called
    // in the initialization of global variables.
    interpStack.ArgsRemove(Bchir::FLAG_SIX); // remove arguments and function value
    interpStack.ArgsPush(IUnit{});
}

void BCHIRInterpreter::InterpretCjTlsSslInit()
{
    // This is a dummy implementation of foreign function CJ_TLS_DYN_SslInit from libs/net/tls/context.cj.
    // We need this dummy implementation because sometimes package net.tls is imported as macro package.
    // CJ_TLS_SslInit is used to initialize global variables.
    interpStack.ArgsRemove(Bchir::FLAG_TWO); // remove argument and function value
    interpStack.ArgsPush(IUnit{});
}

void BCHIRInterpreter::PopsOneReturnsUnitDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::PopsOneReturnsTrueDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(true));
}

void BCHIRInterpreter::PopsOneReturnsFalseDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(false));
}

void BCHIRInterpreter::PopsOneReturnsUInt64ZeroDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IUInt64{0});
}

void BCHIRInterpreter::PopsTwoReturnsUnitDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::PopsTwoReturnsTrueDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(true));
}

void BCHIRInterpreter::PopsThreeReturnsUnitDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::PopsThreeReturnsTrueDummy()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(true));
}

void BCHIRInterpreter::InterpretCRTSTRLEN()
{
    auto iPtr = interpStack.ArgsPop<ITuple>();
    auto ptrVal = IValUtils::Get<IUIntNat>(iPtr.content[0]);
    auto res = strlen(reinterpret_cast<char*>(ptrVal.content));
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IUIntNat>(res));
}

void BCHIRInterpreter::InterpretGetTypeForTypeParameter(Bchir::ByteCodeIndex idx)
{
    auto tyIdx = bchir.Get(idx + Bchir::FLAG_TWO);
    CJC_ASSERT(tyIdx != Bchir::BYTECODE_CONTENT_MAX);
    auto paramTy = bchir.GetTypeAt(static_cast<size_t>(tyIdx));
    (void)paramTy;
    interpStack.ArgsPush(IValUtils::StringToArray(paramTy->ToString()));
}

void BCHIRInterpreter::InterpretCRTMEMCPYS()
{
    auto arg4 = interpStack.ArgsPop<IUIntNat>();
    auto arg3 = interpStack.ArgsPop<ITuple>();
    auto arg2 = interpStack.ArgsPop<IUIntNat>();
    auto arg1 = interpStack.ArgsPop<ITuple>();

    auto destArg = IValUtils::Get<IUIntNat>(arg1.content[0]);
    auto srcArg = IValUtils::Get<IUIntNat>(arg3.content[0]);

    auto res = memcpy_s(
        reinterpret_cast<void*>(destArg.content), arg2.content, reinterpret_cast<void*>(srcArg.content), arg4.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

void BCHIRInterpreter::InterpretCRTMEMSETS()
{
    auto arg4 = interpStack.ArgsPop<IUIntNat>();
    auto arg3 = interpStack.ArgsPop<IInt32>();
    auto arg2 = interpStack.ArgsPop<IUIntNat>();
    auto arg1 = interpStack.ArgsPop<ITuple>();

    auto destArg = IValUtils::Get<IUIntNat>(arg1.content[0]);

    auto res = memset_s(reinterpret_cast<void*>(destArg.content), arg2.content, arg3.content, arg4.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

void BCHIRInterpreter::InterpretCRTFREE()
{
    auto iPtr = interpStack.ArgsPop<ITuple>();
    auto ptrVal = IValUtils::Get<IUIntNat>(iPtr.content[0]);
    free(reinterpret_cast<void*>(ptrVal.content));
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::InterpretCRTMALLOC()
{
    auto arg = interpStack.ArgsPop<IUIntNat>();
    auto res = malloc(arg.content);
    if (res == nullptr) {
        return;
    }
    interpStack.ArgsPush(IValUtils::CreateCPointer(reinterpret_cast<uintptr_t>(res)));
}

void BCHIRInterpreter::InterpretCRTSTRCMP()
{
    auto iPtr2 = interpStack.ArgsPop<ITuple>();
    auto iPtr1 = interpStack.ArgsPop<ITuple>();

    auto ptrVal1 = IValUtils::Get<IUIntNat>(iPtr1.content[0]);
    auto ptrVal2 = IValUtils::Get<IUIntNat>(iPtr2.content[0]);

    auto res = strcmp(reinterpret_cast<char*>(ptrVal1.content), reinterpret_cast<char*>(ptrVal2.content));
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

void BCHIRInterpreter::InterpretCRTMEMCMP()
{
    auto arg3 = interpStack.ArgsPop<IUIntNat>();
    auto arg2 = interpStack.ArgsPop<ITuple>();
    auto arg1 = interpStack.ArgsPop<ITuple>();

    auto sourceArg = IValUtils::Get<IUIntNat>(arg1.content[0]);
    auto targetArg = IValUtils::Get<IUIntNat>(arg2.content[0]);

    auto res =
        memcmp(reinterpret_cast<void*>(sourceArg.content), reinterpret_cast<void*>(targetArg.content), arg3.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

void BCHIRInterpreter::InterpretCRTSTRNCMP()
{
    auto arg3 = interpStack.ArgsPop<IUIntNat>();
    auto arg2 = interpStack.ArgsPop<ITuple>();
    auto arg1 = interpStack.ArgsPop<ITuple>();

    auto sourceArg = IValUtils::Get<IUIntNat>(arg1.content[0]);
    auto targetArg = IValUtils::Get<IUIntNat>(arg2.content[0]);

    auto res =
        strncmp(reinterpret_cast<char*>(sourceArg.content), reinterpret_cast<char*>(targetArg.content), arg3.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

void BCHIRInterpreter::InterpretCRTSTRCASECMP()
{
    auto arg2 = interpStack.ArgsPop<ITuple>();
    auto arg1 = interpStack.ArgsPop<ITuple>();

    auto sourceArg = IValUtils::Get<IUIntNat>(arg1.content[0]);
    auto targetArg = IValUtils::Get<IUIntNat>(arg2.content[0]);

    auto res = strcasecmp(reinterpret_cast<char*>(sourceArg.content), reinterpret_cast<char*>(targetArg.content));
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(res));
}

template <class To, class From> To BitCast(const From& src) noexcept
{
    To dst;
    auto ret = memcpy_s(&dst, sizeof(dst), &src, sizeof(src));
    CJC_ASSERT(ret == EOK);
    return dst;
}

int64_t BCHIRInterpreter::GetArraySize(const IVal& value) const
{
    if (auto array = IValUtils::GetIf<IArray>(&value)) { // this is a normal CHIR array
        return static_cast<int64_t>(array->content.size()) - 1;
    } else { // this is a C-style array
        CJC_ASSERT(std::holds_alternative<ITuple>(value));

        auto& cpointer = IValUtils::Get<ITuple>(value);
        auto ptr = IValUtils::Get<IUIntNat>(cpointer.content[0]).content;
        ptr = ptr - sizeof(int64_t);

        return *BitCast<const int64_t*>(ptr);
    }
}

bool BCHIRInterpreter::InterpretArrayGetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck)
{
    auto popIndex = interpStack.ArgsPop<IInt64>();
    auto arrayPtr = interpStack.ArgsPop<IPointer>();
    return InterpretArrayGet(idx, indexCheck, arrayPtr, popIndex.content);
}

bool BCHIRInterpreter::InterpretArraySliceGetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck)
{
    // INTRINSIC2:: ARRAY_SLICE_GET_ELEMENT :: TYPE_INFO :: OVERFLOW_INFO
    auto overflowStratIdx = idx + Bchir::FLAG_THREE;
    auto overflowStrat = static_cast<OverflowStrategy>(bchir.Get(overflowStratIdx));

    const size_t arrayArgsLen = 3;
    auto popIndex = interpStack.ArgsPop<IInt64>();
    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= arrayArgsLen);
    int64_t res = 0;
    if (InterpretArrayOverflowCheck(idx, structArray, popIndex, overflowStrat, res)) {
        // we should return immediatly to the interpretation loop to continue with the exception
        return true;
    }
    auto& array = IValUtils::Get<IPointer>(structArray.content[0]);
    return InterpretArrayGet(idx, indexCheck, array, res);
}

bool BCHIRInterpreter::InterpretArrayGet(
    Bchir::ByteCodeIndex idx, bool indexCheck, IPointer& arrayPtr, int64_t argIndex)
{
    if (indexCheck && argIndex >= GetArraySize(*arrayPtr.content)) {
        RaiseIndexOutOfBoundsException(idx);
        return true;
    }
    if (auto array = IValUtils::GetIf<IArray>(arrayPtr.content)) { // this is a normal CHIR array
        CJC_ASSERT(argIndex >= 0);
        auto element = array->content[static_cast<size_t>(argIndex) + 1];
        interpStack.ArgsPushIVal(std::move(element));
    }
    return false;
}

bool BCHIRInterpreter::InterpretVArraySetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck)
{
    auto popIndex = interpStack.ArgsPop<IInt64>();
    auto value = interpStack.ArgsPopIVal();
    auto arrayPtr = interpStack.ArgsPop<IPointer>();
    return InterpretVArraySet(idx, indexCheck, arrayPtr, popIndex.content, value);
}

bool BCHIRInterpreter::InterpretArraySetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck)
{
    auto value = interpStack.ArgsPopIVal();
    auto popIndex = interpStack.ArgsPop<IInt64>();
    auto arrayPtr = interpStack.ArgsPop<IPointer>();
    return InterpretArraySet(idx, indexCheck, arrayPtr, popIndex.content, value);
}

bool BCHIRInterpreter::InterpretArraySliceSetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck)
{
    // INTRINSIC2:: ARRAY_SLICE_SET_ELEMENT :: TYPE_INFO :: OVERFLOW_INFO
    auto overflowStratIdx = idx + Bchir::FLAG_THREE;
    auto overflowStrat = static_cast<OverflowStrategy>(bchir.Get(overflowStratIdx));

    const size_t arrayArgsLen = 3;
    auto value = interpStack.ArgsPopIVal();
    auto popIndex = interpStack.ArgsPop<IInt64>();
    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= arrayArgsLen);
    int64_t res = 0;
    if (InterpretArrayOverflowCheck(idx, structArray, popIndex, overflowStrat, res)) {
        // we should return immediatly to the interpretation loop to continue with the exception
        return true;
    }
    auto& array = IValUtils::Get<IPointer>(structArray.content[0]);
    return InterpretArraySet(idx, indexCheck, array, res, value);
}

bool BCHIRInterpreter::InterpretArraySet(
    Bchir::ByteCodeIndex idx, bool indexCheck, IPointer& arrayPtr, int64_t argIndex, const IVal& value)
{
    if (indexCheck && argIndex >= GetArraySize(*arrayPtr.content)) {
        RaiseIndexOutOfBoundsException(idx);
        return true;
    }
    if (auto array = IValUtils::GetIf<IArray>(arrayPtr.content)) { // this is a normal CHIR array
        CJC_ASSERT(argIndex >= 0);
        array->content[static_cast<size_t>(argIndex) + 1] = value;
    }
    interpStack.ArgsPush(IUnit());
    return false;
}

bool BCHIRInterpreter::InterpretVArraySet(
    Bchir::ByteCodeIndex idx, bool indexCheck, const IPointer& arrayPtr, int64_t argIndex, const IVal& value)
{
    auto arraySize = GetArraySize(*arrayPtr.content);
    if (indexCheck && argIndex > arraySize) {
        RaiseIndexOutOfBoundsException(idx);
        return true;
    }
    if (auto array = IValUtils::GetIf<IArray>(arrayPtr.content)) {
        array->content[static_cast<size_t>(argIndex)] = value;
    }
    interpStack.ArgsPush(IUnit());
    return false;
}

void BCHIRInterpreter::InterpretArraySizeIntrinsic()
{
    auto ptr = interpStack.ArgsPop<IPointer>();
    auto size = GetArraySize(*ptr.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt64>(size));
}

void BCHIRInterpreter::InterpretArrayCloneIntrinsic(Bchir::ByteCodeIndex idx)
{
    (void)idx;
    auto arrayPtr = interpStack.ArgsPop<IPointer>();
    if (auto array = IValUtils::GetIf<IArray>(arrayPtr.content)) { // this is a normal CHIR array
        auto newArray = IArray();
        newArray.content = array->content;
        auto newArrayPtr = IPointer();
        newArrayPtr.content = arena.Allocate(std::move(newArray));
        interpStack.ArgsPush(std::move(newArrayPtr));
    }
    return;
}

CHIR::Type* BCHIRInterpreter::GetIntepretArrayElemTy(Bchir::ByteCodeIndex idx) const
{
    size_t tyIdx = bchir.Get(idx + Bchir::FLAG_TWO);
    CJC_ASSERT(tyIdx != Bchir::BYTECODE_CONTENT_MAX);
    auto ty = bchir.GetTypeAt(tyIdx);
    CJC_ASSERT(ty != nullptr);
    auto elemTy = ty->GetTypeArgs()[0];
    CJC_ASSERT(elemTy != nullptr);
    return elemTy;
}

void BCHIRInterpreter::IntepretArrayCopyToIntrinsic(Bchir::ByteCodeIndex idx)
{
    auto copyLenNode = interpStack.ArgsPop<IInt64>();
    auto dstStartNode = interpStack.ArgsPop<IInt64>();
    auto srcStartNode = interpStack.ArgsPop<IInt64>();
    auto dst = interpStack.ArgsPop<IPointer>();
    auto src = interpStack.ArgsPop<IPointer>();
    auto srcStart = srcStartNode.content;
    auto dstStart = dstStartNode.content;
    auto copyLen = copyLenNode.content;
    if (auto srcArray = IValUtils::GetIf<IArray>(src.content)) {
        IntepretArrayNormalCopyToIntrinsic(srcStart, *srcArray, dstStart, dst, idx, copyLen);
    }
    interpStack.ArgsPush(IUnit());
    return;
}

void BCHIRInterpreter::IntepretArrayNormalCopyToIntrinsic(int64_t srcStart, const IArray& srcArray, int64_t dstStart,
    IPointer& dst, Bchir::ByteCodeIndex idx, int64_t copyLen)
{
    (void)idx;
    if (auto dstArray = IValUtils::GetIf<IArray>(dst.content)) {
        auto& srcContent = srcArray.content;
        auto& dstContent = dstArray->content;
        auto srcBegin = srcContent.begin() + srcStart + 1;
        if (dstStart > srcStart) {
            (void)std::copy_backward(srcBegin, srcBegin + copyLen, dstContent.begin() + dstStart + 1 + copyLen);
        } else {
            (void)std::copy(srcBegin, srcBegin + copyLen, dstContent.begin() + dstStart + 1);
        }
    }
}

void BCHIRInterpreter::InterpretObjectZeroValue()
{
    interpStack.ArgsPush(INullptr());
}

void BCHIRInterpreter::InterpretCPointerInitIntrinsic(bool hasArg)
{
    size_t valuePtr = 0;
    if (hasArg) {
        auto srcPointer = interpStack.ArgsPop<ITuple>();
        valuePtr = IValUtils::Get<IUIntNat>(srcPointer.content[0]).content;
    }
    interpStack.ArgsPush(IValUtils::CreateCPointer(valuePtr));
}

void BCHIRInterpreter::InterpretCPointerGetPointerAddressIntrinsic()
{
    auto structTuple = interpStack.ArgsPop<ITuple>();
    auto ptrInt = IValUtils::Get<IUIntNat>(structTuple.content[0]);
    interpStack.ArgsPush(ptrInt);
}

void BCHIRInterpreter::InterpretInvokeGC()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::CopyControlStack(std::vector<Cangjie::CHIR::Interpreter::IVal>& array)
{
    const auto& ctrlStack = interpStack.GetCtrlStack();
    array.reserve(static_cast<unsigned long>(ctrlStack.size() * Bchir::FLAG_THREE));
    // Strictly higher than zero to avoid printing main function
    for (size_t i = ctrlStack.size() - 1; i > 0; i--) {
        auto opCode = ctrlStack[i].opCode;
        if (opCode == OpCode::APPLY || opCode == OpCode::CAPPLY || opCode == OpCode::INVOKE ||
            opCode == OpCode::APPLY_EXC || opCode == OpCode::INVOKE_EXC) {
            auto applyInvoke = IValUtils::PrimitiveValue<IUInt64>(ctrlStack[i].byteCodePtr);
            auto func = IValUtils::PrimitiveValue<IUInt64>(ctrlStack[i - 1].argStackPtr);
            (void)array.emplace_back(applyInvoke);
            (void)array.emplace_back(func);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
            (void)array.emplace_back(IValUtils::PrimitiveValue<IUInt64>(uint64_t(0)));
#endif
        }
    }
    array[0] = IValUtils::PrimitiveValue<IInt64>(static_cast<int64_t>(array.size() - 1));
}

void BCHIRInterpreter::InterpretFillInStackTrace()
{
    interpStack.ArgsPopBack();
    interpStack.ArgsPopBack();
    auto ptr = IPointer();
    auto array = IArray();

    // dummy value to be replaced by the size of the array in CopyControlStack
    (void)array.content.emplace_back(IUnit());
    CopyControlStack(array.content);

    ptr.content = arena.Allocate(std::move(array));
    interpStack.ArgsPush(std::move(ptr));
}

void BCHIRInterpreter::InterpretFillInStackTraceException()
{
    auto exceptionPtr = interpStack.ArgsPop<IPointer>();
    auto& excepObj = IValUtils::Get<IObject>(*exceptionPtr.content);
    auto& traceElementArray = IValUtils::Get<ITuple>(excepObj.content[1]);
    auto& pcArrayPtr = IValUtils::Get<IPointer>(traceElementArray.content[0]);
    auto& pcArray = IValUtils::Get<IArray>(pcArrayPtr.content[0]);

    CopyControlStack(pcArray.content);

    traceElementArray.content[1] = IValUtils::PrimitiveValue<IInt64>(static_cast<int64_t>(pcArray.content.size() - 1));
    interpStack.ArgsPush(IUnit());
}

// Initialize an already allocated array
void BCHIRInterpreter::InterpretArrayInitIntrinsic()
{
    auto len = interpStack.ArgsPop<IInt64>();
    auto elem = interpStack.ArgsPopIVal();
    auto& rawArray = IValUtils::Get<IArray>(*interpStack.ArgsPop<IPointer>().content);
    for (int64_t i = 1; i <= len.content; i++) {
        rawArray.content[static_cast<uint64_t>(i)] = elem;
    }
}

void BCHIRInterpreter::InterpretArraySliceInitIntrinsic()
{
    auto len = interpStack.ArgsPop<IInt64>();
    auto start = interpStack.ArgsPop<IInt64>();
    auto rawArray = interpStack.ArgsPopIVal();
    auto structPtr = interpStack.ArgsPop<IPointer>();
    auto& structArray = IValUtils::Get<ITuple>(*structPtr.content);
    structArray.content = {rawArray, start, len};
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::InterpretArraySliceIntrinsic()
{
    const size_t argsLen = 3;
    auto len = interpStack.ArgsPop<IInt64>();
    auto start = interpStack.ArgsPop<IInt64>();

    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= argsLen);
    CJC_ASSERT(std::holds_alternative<IInt64>(structArray.content[1]));
    auto& arrayStart = IValUtils::Get<IInt64>(structArray.content[1]);

    // ASSUMPTION: slice() function in stdlib has already checked if
    // start + pc has overflow
    int64_t res = start.content + arrayStart.content;

    auto tuple = ITuple{{std::move(structArray.content[0]), IValUtils::PrimitiveValue<IInt64>(res), std::move(len)}};
    interpStack.ArgsPush(std::move(tuple));
}

void BCHIRInterpreter::InterpretArraySliceRawArrayIntrinsic()
{
    const size_t argsLen = 3;
    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= argsLen);
    interpStack.ArgsPushIVal(std::move(structArray.content[0]));
}

void BCHIRInterpreter::InterpretArraySliceStartIntrinsic()
{
    const size_t argsLen = 3;
    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= argsLen);
    interpStack.ArgsPushIVal(std::move(structArray.content[1]));
}

void BCHIRInterpreter::InterpretArraySliceSizeIntrinsic()
{
    const size_t argsLen = 3;
    const size_t lenIndex = 2;
    auto structArray = interpStack.ArgsPop<ITuple>();
    CJC_ASSERT(structArray.content.size() >= argsLen);
    interpStack.ArgsPushIVal(std::move(structArray.content[lenIndex]));
}

bool BCHIRInterpreter::InterpretArrayOverflowCheck(
    Bchir::ByteCodeIndex opIdx, ITuple& structArray, const IInt64& argIndex, OverflowStrategy strategy, int64_t& res)
{
    auto& start = IValUtils::Get<IInt64>(structArray.content[1]);
    bool isOverflow = OverflowChecker::IsOverflowAfterAdd(start.content, argIndex.content, strategy, &res);
    if (strategy == OverflowStrategy::THROWING && isOverflow) {
        RaiseOverflowException(opIdx);
        return true;
    }
    return false;
}

void BCHIRInterpreter::InterpretORD()
{
    auto chr = interpStack.ArgsPop<IRune>().content;
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt32>(int32_t(chr)));
}

void BCHIRInterpreter::InterpretCHR()
{
    auto ui32 = interpStack.ArgsPop<IUInt32>().content;
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IRune>(char32_t(ui32)));
}

void BCHIRInterpreter::InterpretSleep()
{
    auto ns = interpStack.ArgsPop<IInt64>().content;
    const int nano2milli = 1000000;
    std::this_thread::sleep_for(std::chrono::milliseconds(ns / nano2milli));
    interpStack.ArgsPush(IUnit());
}

void BCHIRInterpreter::InterpretRefEq()
{
    auto v1 = interpStack.ArgsPopIVal();
    auto v2 = interpStack.ArgsPopIVal();
    bool temp;
    if (IValUtils::GetIf<INullptr>(&v1) != nullptr) {
        temp = IValUtils::GetIf<INullptr>(&v2) != nullptr;
    } else if (IValUtils::GetIf<INullptr>(&v2) != nullptr) {
        temp = false;
    } else {
        temp = IValUtils::GetIf<IPointer>(&v1)->content == IValUtils::GetIf<IPointer>(&v2)->content;
    }
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IBool>(temp));
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
void BCHIRInterpreter::InterpretDecodeStackTrace()
{
    [[maybe_unused]] auto funcDesc = interpStack.ArgsPop<IUInt64>();
    auto funcStart = interpStack.ArgsPop<IUInt64>();
    auto lpc = interpStack.ArgsPop<IUInt64>();
    auto [mangledName, file, position] = PcFuncToString(lpc, funcStart);
    auto stackTraceData = ITuple();
    stackTraceData.content.reserve(Bchir::FLAG_FOUR);
    auto demangled = BCHIRPrinter::DemangleName(mangledName);
    auto className = IPointer();
    std::string classStr = demangled.first;
    auto classArray = IValUtils::StringToArray(classStr);
    className.content = arena.Allocate(std::move(classArray));
    auto methodName = IPointer();
    std::string methodStr = demangled.second;
    auto methodArray = IValUtils::StringToArray(methodStr);
    methodName.content = arena.Allocate(std::move(methodArray));
    auto fileName = IPointer();
    auto fileArray = IValUtils::StringToArray(file);
    fileName.content = arena.Allocate(std::move(fileArray));
    stackTraceData.content.emplace_back(className);
    stackTraceData.content.emplace_back(methodName);
    stackTraceData.content.emplace_back(fileName);
    stackTraceData.content.emplace_back(IValUtils::PrimitiveValue<IInt64>(position));
    interpStack.ArgsPush(std::move(stackTraceData));
}
#endif

/**
 * @brief Creates an tuple containing an array with the representation of a string
 */
ITuple BCHIRInterpreter::StringToITuple(const std::string& str)
{
    auto tuple = ITuple();
    auto tuplePtrArr = IPointer();
    auto tupleArray = IValUtils::StringToArray(str);
    tuplePtrArr.content = arena.Allocate(std::move(tupleArray));
    tuple.content.reserve(Bchir::FLAG_TWO);
    tuple.content.emplace_back(tuplePtrArr);
    tuple.content.emplace_back(IValUtils::PrimitiveValue<IInt64>(static_cast<int64_t>(tupleArray.content.size())));
    return tuple;
}

std::tuple<std::string, std::string, int> BCHIRInterpreter::PcFuncToString(
    const IUInt64 paramPc, const IUInt64 funcStart) const
{
    auto applyPosition =
        bchir.GetLinkedByteCode().GetCodePositionAnnotation(static_cast<Bchir::ByteCodeIndex>(paramPc.content));
    auto mangledName =
        bchir.GetLinkedByteCode().GetMangledNameAnnotation(static_cast<Bchir::ByteCodeIndex>(funcStart.content));
    auto file = bchir.GetFileName(applyPosition.fileID);
    auto position = applyPosition.line;
    return {mangledName, file, position};
}

void BCHIRInterpreter::InterpretCStringInit()
{
    // OPTIMIZE: we don't need this once we make the interpreter values match the FFI values
    auto tuple = interpStack.ArgsPop<ITuple>();
    // case when it is passed a CPointer
    if (std::holds_alternative<IUIntNat>(tuple.content[0])) {
        interpStack.ArgsPush(tuple);
        return;
    }

    // case when it is passed a string
    auto& ptr = IValUtils::Get<IPointer>(tuple.content[0]);

    // we copy the string here because that is how cjnative backend deal with it
    if (auto array = IValUtils::GetIf<IArray>(ptr.content)) {
        auto& size = IValUtils::Get<IInt64>(array->content[0]);
        // according to the code in the standard library, when a CString is initialized with a string
        // the user is responsible to manually free it.
        auto const str = static_cast<uint8_t*>(calloc(static_cast<size_t>(size.content + 1), 1));
        if (str == nullptr) {
            CJC_ABORT();
            return;
        }
        CJC_ASSERT(size.content == IValUtils::Get<IInt64>(array->content[0]).content);
        for (auto i = 1; i <= size.content; ++i) {
            str[i - 1] = IValUtils::Get<IUInt8>(array->content[static_cast<size_t>(i)]).content;
        }
        auto cstring = IValUtils::CreateCPointer(BitCast<size_t>(str));
        interpStack.ArgsPush(std::move(cstring));
    } else {
        CJC_ASSERT(std::holds_alternative<ITuple>(*ptr.content));
        auto value = IValUtils::Get<ITuple>(*ptr.content).content[0];
        CJC_ASSERT(std::holds_alternative<IUIntNat>(value));
        const auto cptr = BitCast<uint8_t*>(IValUtils::Get<IUIntNat>(value).content);
        size_t size =
            static_cast<size_t>(*(BitCast<int64_t*>(IValUtils::Get<IUIntNat>(value).content - sizeof(int64_t))));
        // according to the code in the standard library, when a CString is initialized with a string
        // the user is responsible to manually free it.
        const auto str = static_cast<uint8_t*>(calloc(size + 1, 1));
        if (str == nullptr) {
            CJC_ABORT();
            return;
        }
        auto ret = memcpy_s(str, size + 1, cptr, size);
        if (ret != EOK) {
            CJC_ABORT();
            return;
        }
        str[size] = 0;
        auto cstring = IValUtils::CreateCPointer(BitCast<size_t>(str));
        interpStack.ArgsPush(std::move(cstring));
    }
}

void BCHIRInterpreter::InterpretconvertCStr2Ptr() const
{
    CJC_ASSERT(std::holds_alternative<ITuple>(interpStack.ArgsTopIVal()));
}

void BCHIRInterpreter::InterpretIdentityHashCode()
{
    auto r = BitCast<int64_t>(interpStack.ArgsPop<IPointer>().content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt64>(r));
}

void BCHIRInterpreter::InterpretIdentityHashCodeForArray()
{
    auto arrTuple = interpStack.ArgsPop<ITuple>();
    auto arrPointer = IValUtils::Get<IPointer>(arrTuple.content[0]);
    auto r = BitCast<int64_t>(arrPointer.content);
    interpStack.ArgsPush(IValUtils::PrimitiveValue<IInt64>(r));
}

void BCHIRInterpreter::InterpretAtomicLoad()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto ival = atomic.content[0];
    interpStack.ArgsPushIVal(std::move(ival));
}

void BCHIRInterpreter::InterpretAtomicStore()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPopIVal();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    atomic.content[0] = val;
    interpStack.ArgsPush(IUnit{});
}

void BCHIRInterpreter::InterpretAtomicSwap()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPopIVal();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto ival = atomic.content[0];
    interpStack.ArgsPushIVal(std::move(ival));
    atomic.content[0] = val;
}

template <typename IValType> void BCHIRInterpreter::InterpretAtomicCAS()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto newVal = interpStack.ArgsPopIVal();
    auto oldVal = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    if constexpr (std::is_same<IValType, ITuple>::value) { // option type
        auto& oldIdx = IValUtils::Get<IBool>(oldVal.content[0]);
        auto& curIdx = IValUtils::Get<IBool>(curVal.content[0]);
        if (curIdx.content != oldIdx.content) {
            interpStack.ArgsPush(IBool{false});
            return;
        }
        if (oldIdx.content == 0) {
            auto& oldPtr = IValUtils::Get<IPointer>(oldVal.content[1]);
            auto& curPtr = IValUtils::Get<IPointer>(curVal.content[1]);
            if (curPtr.content != oldPtr.content) {
                interpStack.ArgsPush(IBool{false});
                return;
            }
        }
    } else {
        if (curVal.content != oldVal.content) {
            interpStack.ArgsPush(IBool{false});
            return;
        }
    }
    atomic.content[0] = newVal;
    interpStack.ArgsPush(IBool{true});
}

template <typename IValType, typename NumType> void BCHIRInterpreter::InterpretAtomicFetchAdd()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    NumType newVal = curVal.content + val.content;
    atomic.content[0] = IValType{newVal};
    interpStack.ArgsPush(curVal);
}

template <typename IValType, typename NumType> void BCHIRInterpreter::InterpretAtomicFetchSub()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    NumType newVal = curVal.content - val.content;
    atomic.content[0] = IValType{newVal};
    interpStack.ArgsPush(curVal);
}

template <typename IValType, typename NumType> void BCHIRInterpreter::InterpretAtomicFetchAnd()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    NumType newVal = curVal.content & val.content;
    atomic.content[0] = IValType{newVal};
    interpStack.ArgsPush(curVal);
}

template <typename IValType, typename NumType> void BCHIRInterpreter::InterpretAtomicFetchOr()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    NumType newVal = curVal.content | val.content;
    atomic.content[0] = IValType{newVal};
    interpStack.ArgsPush(curVal);
}

template <typename IValType, typename NumType> void BCHIRInterpreter::InterpretAtomicFetchXor()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    interpStack.ArgsPopBack(); // this memory order
#endif
    auto val = interpStack.ArgsPop<IValType>();
    auto atomicPtr = interpStack.ArgsPop<IPointer>();
    auto& atomic = IValUtils::Get<IObject>(*atomicPtr.content);
    auto curVal = IValUtils::Get<IValType>(atomic.content[0]);
    NumType newVal = curVal.content ^ val.content;
    atomic.content[0] = IValType{newVal};
    interpStack.ArgsPush(curVal);
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
void BCHIRInterpreter::InterpretVectorIndexByte32()
{
    auto target = interpStack.ArgsPop<IUInt8>();
    auto offset = interpStack.ArgsPop<IInt64>();
    auto arrayPtr = interpStack.ArgsPop<IPointer>();
    if (auto array = IValUtils::GetIf<IArray>(arrayPtr.content); array != nullptr) { // this is a normal CHIR array
        int64_t idx = offset.content + 1;                          // first position contains the size of the array
        for (; static_cast<size_t>(idx) < array->content.size(); ++idx) {
            if (IValUtils::Get<IUInt8>(array->content[static_cast<size_t>(idx)]).content == target.content) {
                break;
            }
        }
        interpStack.ArgsPush(IInt64{idx});
    } else { // this is a C-style array
        auto& cpointer = IValUtils::Get<ITuple>(*arrayPtr.content);
        auto ptr = IValUtils::Get<IUIntNat>(cpointer.content[0]).content;
        auto size = *BitCast<const int64_t*>(ptr - sizeof(int64_t));
        auto uint8Ptr = BitCast<const uint8_t*>(ptr);
        auto it = std::find(uint8Ptr + offset.content, uint8Ptr + size, target.content);
        auto idx = static_cast<int64_t>(it - uint8Ptr);
        interpStack.ArgsPush(IInt64{idx});
    }
}
#endif