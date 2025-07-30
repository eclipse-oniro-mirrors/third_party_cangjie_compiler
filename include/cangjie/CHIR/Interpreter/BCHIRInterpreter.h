// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the interpreter for BCHIR.
 */

#ifndef CANGJIE_CHIR_INTERRETER_BCHIRINTERPRETER_H
#define CANGJIE_CHIR_INTERRETER_BCHIRINTERPRETER_H

// we need to undefine X86_64 and AARCH64 because of include/cangjie/Option/Option.h
#if defined(X86_64)
#undef X86_64
#endif
#if defined(AARCH64)
#undef AARCH64
#endif
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/OverflowChecking.h"
#include "cangjie/CHIR/Interpreter/BCHIR.h"
#include "cangjie/CHIR/Interpreter/BCHIRPrinter.h"
#include "cangjie/CHIR/Interpreter/BCHIRResult.h"
#include "cangjie/CHIR/Interpreter/InterpreterArena.h"
#include "cangjie/CHIR/Interpreter/InterpreterEnv.h"
#include "cangjie/CHIR/Interpreter/InterpreterStack.h"
#include "cangjie/CHIR/Interpreter/InterpreterValue.h"
#include <cmath>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
// we need to undefine THIS, INTERFACE, and FASTCALL which are defined by MinGW
#if defined(THIS)
#undef THIS
#endif
#if defined(INTERFACE)
#undef INTERFACE
#endif
#if defined(interface)
#undef interface
#endif
#if defined(CONST)
#undef CONST
#endif
#if defined(GetObject)
#undef GetObject
#endif
#if defined(FASTCALL)
#undef FASTCALL
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace Cangjie::CHIR::Interpreter {

class BCHIRInterpreter {
public:
    BCHIRInterpreter(Bchir& bchir, DiagnosticEngine& diag, const std::unordered_map<std::string, void*>& dyHandles,
        Bchir::ByteCodeIndex playgroundIdxBase, Bchir::ByteCodeIndex externalPlaygroundIdx, bool isConstEval = false)
        : bchir(bchir),
          env(bchir.GetNumGlobalVars()),
          dyHandles(dyHandles),
          playgroundIdxBase(playgroundIdxBase),
          externalPlaygroundIdx(externalPlaygroundIdx),
          isConstEval(isConstEval),
          diag(diag)
    {
    }

    ~BCHIRInterpreter()
    {
        ExecuteFinalizers();
    }

    /** @brief runt the interpreter */
    IResult Run(size_t baseIdx, bool expectsReturn = true);

    /** @brief access the Bchir being evaluated */
    const Bchir& GetBchir() const;

    /** @brief Moves a value to the arena, and returns the pointer */
    IPointer ToArena(IVal&& value);

    /** @brief push value into the interpreter argument stack */
    void PushIntoArgStack(IVal ival);

    /** @brief get the value of a global variable */
    const IVal& PeekValueOfGlobal(Bchir::VarIdx id) const;

    /** @brief set the global vars in the environment according to `gVarInitVals` */
    void SetGlobalVars(std::unordered_map<Bchir::ByteCodeIndex, IVal>&& gVarInitIVals);

    /** @brief execute all finalizers */
    void ExecuteFinalizers() noexcept;

    /** @brief returns the result of the previous run, or INotRun if interpreter never ran */
    const IResult& GetLastResult() const;

    std::pair<std::string, Cangjie::Position> GetBacktraceForConstEval(const IException& exnPtr);

    /** @brief the max size of the internal playground, the part of the bytecode
     * where this interpreter instance can generate code. */
    static const size_t INTERNAL_PLAYGROUND_SIZE = 20;

    /** @brief the max size of the external playground, the part of the bytecode
     * where users of this instance can generate code. */
    static const size_t EXTERNAL_PLAYGROUND_SIZE = 20;

    /** @brief Debug utility. Return a string with the code position for a bytecode operation index. */
    std::string DebugGetPosition(Bchir::ByteCodeIndex index);
    /** @brief Debug utility. Return a string with the mangled for a bytecode operation index. */
    std::string DebugGetMangledName(Bchir::ByteCodeIndex index) const;

#ifndef NDEBUG
    /** @brief Check if writing debug data to a file is enabled. If so, create output file for PrintDebugInfo. */
    void PrepareRuntimeDebug(const GlobalOptions& options);
    /** @brief Debug utility. Write information about current pc to file (if enabled). */
    void PrintDebugInfo(Bchir::ByteCodeIndex currentPc);
#endif

    // simply storing the main argument strings here
    // for CJ_GetMainArgs to query
    std::vector<std::string> mainArgs;

private:
    /** @brief the bytecode to be interpreted */
    Bchir& bchir;
    /** @brief interpreter stack */
    InterpreterStack interpStack;
    /* The way the environment works is as follows (note that this approach only works for
    closure converted OP code):
    1. when *entering a function* we should StartStackFrame (store current base pointer and set
    it to the top of the local environment stack).
    2. when *exiting a function* we should RestoreStackFrame (restore the base pointer to the
    previous one).
    3. when entering thunks that are not function's thunks, we just (possibly) emplace
    new variables in the local environment stack (see SetLocal implementation for details).
    4. when exiting thunks that are not function's thunks we simply do nothing. */
    /** @brief environment for local and global variables */
    Env env;
    /* represents the heap */
    Arena arena;

    /** @brief available dynamic libs to load syscall functions */
    const std::unordered_map<std::string, void*>& dyHandles;

    /** @brief true if an unrecoverable error occurred in the interpreter */
    bool interpreterError = false;

    /** @brief program counter */
    Bchir::ByteCodeIndex pc = 0;

    /** @brief base index, that is, the index where the interpretation starts */
    Bchir::ByteCodeIndex baseIndex = 0;
    /** @brief the index of the playground where this interpreter can generate bytecode.
    The size of the payground is given by INTERNAL_PLAYGROUND_SIZE. */
    Bchir::ByteCodeIndex playgroundIdxBase;
    /** @brief the value of playgroundIdx is always set to playgroundIdxBase when Run is executed and is updated
     * accordingly when bytecode is generated by the runtime. */
    Bchir::ByteCodeIndex playgroundIdx;
    /** @brief index of the external playground.
     *
     * External entities can write to this playground and the interpreter should never change it. Normally external
     * entities will write bytecode to be evaluated by the interpreter. E.g.
     *
     * APPLY :: SOME_IDX :: NUMBER_OF_ARGS
     *
     * and then call Run(externalPlayground). */
    Bchir::ByteCodeIndex externalPlaygroundIdx;

    bool raiseExnToTopLevel = false;
    std::optional<IPointer> exception{};

    // Is the interpreter being used for constant evaluation?
    bool isConstEval = false;
    /** @brief interpreter last result */
    IResult result{INotRun{}};

#ifndef NDEBUG
    bool printRuntimeDebugInfo{false};
    std::fstream debugFile;
#endif

    void Interpret();
    void InterpretString();
    template <OpCode op = OpCode::APPLY> void InterpretApply();
    template <OpCode op = OpCode::INVOKE> void InterpretInvoke();
    void InterpretDeref();
    void InterpretSyscall();
    template <OpCode op = OpCode::TYPECAST> void InterpretTypeCast();
    /** @brief Returns true if exception is raised. */
    bool InterpretIntrinsic0();
    /** @brief Returns true if exception is raised. */
    bool InterpretIntrinsic1();
    /** @brief Returns true if exception is raised. */
    bool InterpretIntrinsic2();
    void InterpretArrayBuilder();
    void InterpretRawArrayInitByValue();
    void InterpretVArrayByValue();
    template <bool isLiteral, bool isExc> void InterpretAllocateRawArray();
    template <OpCode op = OpCode::RAISE> void InterpretRaise();
    void InterpretSwitch();
    void InterpretStoreInRef();
    void InterpretGetRef();
    void InterpretFieldTpl();
    void InterpretReturn();
    void PopsOneReturnsUnitDummy();
    void PopsOneReturnsTrueDummy();
    void PopsOneReturnsFalseDummy();
    void PopsOneReturnsUInt64ZeroDummy();
    void PopsTwoReturnsUnitDummy();
    void PopsTwoReturnsTrueDummy();
    void PopsThreeReturnsUnitDummy();
    void PopsThreeReturnsTrueDummy();

    IVal* AllocateValue(IVal&& value);

    bool FindCatchInCtrlStack();

    // Invoke support
    Bchir::ByteCodeIndex FindMethod(Bchir::ByteCodeContent classId, Bchir::ByteCodeContent nameId);

    // Switch support
    template <typename Ty> void InterpretSwitchWithType();

    // Instanceof support
    bool IsSubclass(Bchir::ByteCodeContent lhs, Bchir::ByteCodeContent rhs);

    // Raise exceptions
    // be careful when using these functions, they should return to
    // the main loop immediate
    void RaiseArithmeticException(Bchir::ByteCodeIndex sourcePc);
    void RaiseOverflowException(Bchir::ByteCodeIndex sourcePc);
    void RaiseIndexOutOfBoundsException(Bchir::ByteCodeIndex sourcePc);
    void RaiseNegativeArraySizeException(Bchir::ByteCodeIndex sourcePc);
    void RaiseArithmeticExceptionMsg(Bchir::ByteCodeIndex sourcePc, const std::string& str);
    void RaiseOutOfMemoryError(Bchir::ByteCodeIndex sourcePc);
    void RaiseError(Bchir::ByteCodeIndex sourcePc, const std::string& str);
    void CallPrintStackTrace(IVal&& exnPtr);
    void CallPrintStackTraceError(IVal&& exnPtr);
    void InterpretCJTLSDYNSetSessionCallback();
    void InterpretCjTlsSslInit();

    void CallToString(IVal&& exnPtr);
    ITuple StringToITuple(const std::string& str);
    std::tuple<std::string, std::string, int> PcFuncToString(const IUInt64 paramPc, const IUInt64 funcStart) const;
    void CheckIsError(IVal&& exnPtr);
    // args does not include func-op
    void CallFunction(size_t callIdx, std::vector<IVal> args);

    /* Binary operations */
    /** @brief Perform binary operation */
    template <OpCode op> void BinOp();
    /** @brief Binary operation for TypeKind and Overflow strategy. Returns true if excetion is raised. */
    template <OpCode op> bool BinOpTyKindAndOverflowStrat(CHIR::Type::TypeKind kind, Cangjie::OverflowStrategy strat);
    /** @brief Perform binary operation with a fixed boolean type */
    template <OpCode op> void BinOpFixedBool();

    /** @brief Perform binary operation auxiliar for integral types. Returns true if excetion is raised. */
    template <OpCode op, typename T, typename S> bool BinOpInt(Cangjie::OverflowStrategy strat);
    /** @brief Binary exponential. Returns true if excetion is raised. */
    template <typename T, typename S> bool BinExpOpInt(Cangjie::OverflowStrategy strat);
    /** @brief Perform regular arithmetic operations: ADD, SUB, MUL, DIV, MOD, BITAND, BITOR, BITXOR, LT, LE, GT, GE,
     * EQ, NEQ. Returns true if excetion is raised. */
    template <OpCode op, typename T, typename S> bool BinRegOpInt(Cangjie::OverflowStrategy strat);
    /** @brief Perform shift operation. Returns true if excetion is raised. */
    template <typename Type, typename T, typename S, OpCode op> bool BinShiftOpIntCase();

    /** @brief Perform shift operation. Returns true if excetion is raised. */
    template <OpCode op, typename T, typename S> bool BinShiftOpInt();
    /** @brief Adds res to the argument stack or raises exception. Returns true if excetion is raised. */
    template <typename T, typename S> bool PushIfNotOverflow(bool overflow, S res, Cangjie::OverflowStrategy strat);
    /** @brief Perform binary operation auxiliar for floating point types. Returns true if excetion is raised. */
    template <OpCode op, typename T, typename S> bool BinOpFloat();
    /** @brief Perform binary operation for bools. Returns true if excetion is raised. */
    template <OpCode op> bool BinOpBool();
    /** @brief Perform binary operation for Rune. Returns true if excetion is raised. */
    template <OpCode op> bool BinOpRune();
    /** @brief Perform binary operation for Unit. Returns true if excetion is raised. */
    template <OpCode op> bool BinOpUnit();

    /** @brief Perform binary compare operation. Returns true if excetion is raised. */
    template <OpCode op, typename T> bool BinOpCompare(T x, T y);

    /* FFI utilities */
    /** @brief Calculates the size required to store the C type corresponding to `ty`. */
    unsigned long SizeOfFFIType(const CHIR::Type& ty);
    /** @brief Calculates the alignment of the C type corresponding to `ty`. */
    unsigned long AlignOfFFIType(const CHIR::Type& ty);
    /** @brief returns true on success, false otherwise */
    /* we just pass the resultSize to make the code quality check happy */
    bool ExecuteSyscall(void (*func)(void), size_t numberOfArgs, void* execResult);

    void PerformSyscall(
        const std::string& name, Bchir::ByteCodeIndex opIdx, void (*func)(void), size_t args, const CHIR::Type& resTy);

    /* Intrinsic functions */
    template <OpCode op> void InterpretIntrinsic();
    template <Cangjie::CHIR::IntrinsicKind, bool IsSyscall = false> void InterpretASTIntrinsicWithFFI();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForParseExpr();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForParseDecl();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForParseType();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForParsePattern();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForParentContext();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForMacroSetItem();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForGetChildMessage();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForCheckAddSpace();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForDiagReport();
    template <bool IsSyscall = false> void InterpretIntrinsicWithFFIForCangjieLex();
    void InterpretCRTSTRLEN();
    void InterpretSizeOf(Bchir::ByteCodeIndex idx);
    void InterpretAlignOf(Bchir::ByteCodeIndex idx);
    void InterpretGetTypeForTypeParameter(Bchir::ByteCodeIndex idx);
    void InterpretCRTMEMCPYS();
    void InterpretCRTMEMSETS();
    void InterpretCRTFREE();
    void InterpretCRTMALLOC();
    void InterpretCRTSTRCMP();
    void InterpretCRTMEMCMP();
    void InterpretCRTSTRNCMP();
    void InterpretCRTSTRCASECMP();
    int64_t GetArraySize(const IVal& value) const;
    void InterpretArraySizeIntrinsic();
    void InterpretArrayCloneIntrinsic(Bchir::ByteCodeIndex idx);
    void InterpretObjectZeroValue();
    CHIR::Type* GetIntepretArrayElemTy(Bchir::ByteCodeIndex idx) const;
    void IntepretArrayCopyToIntrinsic(Bchir::ByteCodeIndex idx);
    void IntepretArrayNormalCopyToIntrinsic(int64_t srcStart, const IArray& srcArray, int64_t dstStart, IPointer& dst,
        Bchir::ByteCodeIndex idx, int64_t copyLen);
    void IntepretArrayCStyleCopyToIntrinsic(int64_t srcStart, const IPointer& src, int64_t dstStart, IPointer& dst,
        Bchir::ByteCodeIndex idx, int64_t copyLen);
    void InterpretAcquireRawDataIntrinsic(Bchir::ByteCodeIndex idx);
    void InterpretReleaseRawDataIntrinsic();
    void InterpretCPointerPlusIntrinsic(Bchir::ByteCodeIndex idx);
    void InterpretCPointerReadIntrinsic(Bchir::ByteCodeIndex idx);
    void InterpretCPointerGetPointerAddressIntrinsic();
    void InterpretCPointerInitIntrinsic(bool hasArg = false);
    void InterpretCPointerWriteIntrinsic(Bchir::ByteCodeIndex idx);
    void InterpretFillInStackTrace();
    void CopyControlStack(std::vector<Cangjie::CHIR::Interpreter::IVal>& array);
    void InterpretFillInStackTraceException();
    void InterpretArrayInitIntrinsic();
    void InterpretArraySliceInitIntrinsic();
    void InterpretArraySliceIntrinsic();
    void InterpretArraySliceRawArrayIntrinsic();
    void InterpretArraySliceStartIntrinsic();
    void InterpretArraySliceSizeIntrinsic();
    bool InterpretArrayOverflowCheck(Bchir::ByteCodeIndex opIdx, ITuple& structArray, const IInt64& argIndex,
        OverflowStrategy strategy, int64_t& res);
    /** @brief Returns true if exception is raised. */
    bool InterpretArrayGetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck);
    /** @brief Returns true if exception is raised. */
    bool InterpretArraySliceGetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck);
    /** @brief Returns true if exception is raised. */
    bool InterpretArrayGet(Bchir::ByteCodeIndex idx, bool indexCheck, IPointer& arrayPtr, int64_t argIndex);
    void InterpretVArrayGet();
    /** @brief Returns true if exception is raised. */
    bool InterpretArraySetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck);
    /** @brief Returns true if exception is raised. */
    bool InterpretVArraySetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck);
    /** @brief Returns true if exception is raised. */
    bool InterpretArraySliceSetIntrinsic(Bchir::ByteCodeIndex idx, bool indexCheck);
    /** @brief Returns true if exception is raised. */
    bool InterpretArraySet(
        Bchir::ByteCodeIndex idx, bool indexCheck, IPointer& arrayPtr, int64_t argIndex, const IVal& value);
    /** @brief Returns true if exception is raised. */
    bool InterpretVArraySet(
        Bchir::ByteCodeIndex idx, bool indexCheck, const IPointer& arrayPtr, int64_t argIndex, const IVal& value);
    void InterpretRawArrayLiteralInit();
    void InterpretORD();
    void InterpretCHR();
    void InterpretSleep();
    void InterpretRefEq();
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    void InterpretDecodeStackTrace();
    void InterpretCJCodeCanUseSIMD();
    void InterpretVectorIndexByte32();
    void ReportConstEvalException(Bchir::ByteCodeIndex opIdx, std::string exceptionName);
#endif

    void InterpretCStringInit();
    void InterpretconvertCStr2Ptr() const;
    void InterpretIdentityHashCode();
    void InterpretIdentityHashCodeForArray();
    void InterpretInvokeGC();
    void InterpretAtomicLoad();
    void InterpretAtomicStore();
    void InterpretAtomicSwap();
    template <typename IValType> void InterpretAtomicCAS();
    template <typename IValType, typename NumType> void InterpretAtomicFetchAdd();
    template <typename IValType, typename NumType> void InterpretAtomicFetchSub();
    template <typename IValType, typename NumType> void InterpretAtomicFetchAnd();
    template <typename IValType, typename NumType> void InterpretAtomicFetchOr();
    template <typename IValType, typename NumType> void InterpretAtomicFetchXor();

    /* TypeCast */
    template <typename SourceTyRaw, typename TargetTy, typename TargetTyRaw>
    bool CastOrRaiseExceptionForInt(SourceTyRaw v, OverflowStrategy strategy, Bchir::ByteCodeIndex opIdx);
    template <typename SourceTyRaw, typename TargetTy, typename TargetTyRaw>
    bool CastOrRaiseExceptionForFloat(SourceTyRaw floatVal, Bchir::ByteCodeIndex opIdx);
    template <typename T>
    bool InterpretTypeCastForInt(
        T val, CHIR::Type::TypeKind targetKind, OverflowStrategy strategy, Bchir::ByteCodeIndex opIdx);
    template <typename T>
    bool InterpretTypeCastForFloat(T floatVal, CHIR::Type::TypeKind targetKind, Bchir::ByteCodeIndex opIdx);

    /* ERROR handling */
    /** @brief terminate interpretation due to unexpected error.
     *
     * @param opIdx for the operation that caused the failure. We use it to extract the position.
     */
    DiagnosticEngine& diag;
    template <typename... Args>
    void FailWith(Bchir::ByteCodeIndex opIdx, std::string excErrorMsg, DiagKind kind, Args... args)
    {
        auto& sm = diag.GetSourceManager();
        auto pos = bchir.GetLinkedByteCode().GetCodePositionAnnotation(opIdx);
        // convert file name (bchir) ID to (source manager) ID
        auto fileName = bchir.GetFileName(pos.fileID);
        auto fileId = sm.GetFileID(fileName);
        if (fileId == -1) {
            fileId = static_cast<int>(sm.AddSource(fileName, ""));
        }
        Cangjie::Position cjPos{
            static_cast<unsigned int>(fileId), static_cast<int>(pos.line), static_cast<int>(pos.column)};
        if (cjPos.IsZero()) {
            diag.Diagnose(kind, args...);
        } else {
            diag.Diagnose(cjPos, kind, args...);
        }
        RaiseError(opIdx, excErrorMsg);
    }
};

} // namespace Cangjie::CHIR::Interpreter

#endif // CANGJIE_CHIR_INTERRETER_BCHIRINTERPRETER_H
