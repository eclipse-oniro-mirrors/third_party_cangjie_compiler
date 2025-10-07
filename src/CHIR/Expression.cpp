// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements the Expression class in CHIR.
 */

#include "cangjie/CHIR/Expression.h"

#include <iostream>
#include <sstream>

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/CHIR/ToStringUtils.h"
#include "cangjie/CHIR/Type/ClassDef.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/CHIR/Value.h"
#include "cangjie/CHIR/Visitor/Visitor.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie::CHIR;

const ExprKindMgr* ExprKindMgr::Instance()
{
    static ExprKindMgr ins;
    return &ins;
}

/*
    * @brief: Get major kind which the `exprKind` belongs to
    *
    * @param exprKind: defined in file "cangjie/CHIR/ExprKind.inc"
    *
    */
ExprMajorKind ExprKindMgr::GetMajorKind(ExprKind exprKind) const
{
    return expr2MajorExprMap.at(exprKind);
}

/*
    * @brief: Get kind name
    *
    * @param exprKind: defined in file "cangjie/CHIR/ExprKind.inc"
    *
    */
std::string ExprKindMgr::GetKindName(size_t exprKind) const
{
    return exprKindNames[exprKind];
}

void ExprKindMgr::InitMap(ExprMajorKind majorKind, ...)
{
    va_list vaList;
    va_start(vaList, majorKind);
    ExprKind arg = va_arg(vaList, ExprKind);
    while (static_cast<size_t>(arg) != 0) {
        expr2MajorExprMap[arg] = majorKind;
        arg = va_arg(vaList, ExprKind);
    }
    va_end(vaList);
}

ExprKindMgr::ExprKindMgr()
{
#define EXPRKIND(KIND, ...) ExprKind::KIND,
#define EXPRKINDS(...) __VA_ARGS__
#define MAJORKIND(KIND, ...) InitMap(ExprMajorKind::KIND, EXPRKINDS(__VA_ARGS__) ExprKind::INVALID);
#include "cangjie/CHIR/ExprKind.inc"
#undef EXPRKIND
#undef EXPRKINDS
#undef MAJORKIND
}

Expression::Expression(
    ExprKind kind, const std::vector<Value*>& operands, const std::vector<BlockGroup*>& blockGroups, Block* parent)
    : kind(kind), operands(operands), blockGroups(blockGroups), parent(parent)
{
    CJC_NULLPTR_CHECK(parent);
    for (auto op : operands) {
        // note: a hack here, remove later
        if (op == nullptr) {
            continue;
        }
        op->AddUserOnly(this);
    }
    for (auto blockGroup : blockGroups) {
        blockGroup->AddUserOnly(this);
    }
}

bool Expression::IsConstantNull() const
{
    if (kind == ExprKind::CONSTANT) {
        auto constant = StaticCast<const Constant*>(this);
        return constant->IsNullLit();
    }
    return false;
}

bool Expression::IsConstantInt() const
{
    if (kind == ExprKind::CONSTANT) {
        auto constant = StaticCast<const Constant*>(this);
        return constant->IsIntLit();
    }
    return false;
}

bool Expression::IsConstantFloat() const
{
    if (kind == ExprKind::CONSTANT) {
        auto constant = StaticCast<const Constant*>(this);
        return constant->IsFloatLit();
    }
    return false;
}

bool Expression::IsConstantBool() const
{
    if (kind == ExprKind::CONSTANT) {
        auto constant = StaticCast<const Constant*>(this);
        return constant->IsBoolLit();
    }
    return false;
}

bool Expression::IsConstantString() const
{
    if (kind == ExprKind::CONSTANT) {
        auto constant = StaticCast<const Constant*>(this);
        return constant->IsStringLit();
    }
    return false;
}

ExprMajorKind Expression::GetExprMajorKind() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind);
}

ExprKind Expression::GetExprKind() const
{
    return kind;
}

bool Expression::IsConstant() const
{
    return kind == ExprKind::CONSTANT;
}

bool Expression::IsTerminator() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::TERMINATOR;
}

bool Expression::IsUnaryExpr() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::UNARY_EXPR;
}

bool Expression::IsBinaryExpr() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::BINARY_EXPR;
}

bool Expression::IsMemoryExpr() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::MEMORY_EXPR;
}

bool Expression::IsStructuredControlFlowExpr() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::STRUCTURED_CTRL_FLOW_EXPR;
}

bool Expression::IsOtherExpr() const
{
    return ExprKindMgr::Instance()->GetMajorKind(kind) == ExprMajorKind::OTHERS;
}

std::string Expression::GetExprKindName() const
{
    return ExprKindMgr::Instance()->GetKindName(static_cast<size_t>(kind));
}

std::pair<ExprMajorKind, ExprKind> Expression::GetMajorAndMinorExprKind() const
{
    return {ExprKindMgr::Instance()->GetMajorKind(kind), kind};
}

Block* Expression::GetParent() const
{
    return parent;
}

BlockGroup* Expression::GetBlockGroup(unsigned idx) const
{
    CJC_ASSERT(idx < blockGroups.size());
    return blockGroups[idx];
}

size_t Expression::GetNumOfBlockGroups() const
{
    return blockGroups.size();
}

const std::vector<BlockGroup*>& Expression::GetBlockGroups() const
{
    return blockGroups;
}

size_t Expression::GetNumOfOperands() const
{
    return operands.size();
}

std::vector<Value*> Expression::GetOperands() const
{
    return operands;
}

Value* Expression::GetOperand(size_t idx) const
{
    CJC_ASSERT(idx < operands.size());
    return operands[idx];
}

LocalVar* Expression::GetResult() const
{
    return result;
}

Type* Expression::GetResultType() const
{
    return result ? result->GetType() : nullptr;
}

bool Expression::IsCompileTimeValue() const
{
    return isCompileTimeValue;
}

void Expression::SetCompileTimeValue()
{
    isCompileTimeValue = true;
}

/**
 * @brief Get the block group to which the expression belongs.
 */
BlockGroup* Expression::GetParentBlockGroup() const
{
    if (parent == nullptr) {
        return nullptr;
    }
    return parent->GetParentBlockGroup();
}

Func* Expression::GetParentFunc() const
{
    if (auto blockGroup = GetParentBlockGroup(); blockGroup != nullptr) {
        return blockGroup->GetParentFunc();
    }
    return nullptr;
}

/**
 * @brief Remove this expression from its parent basicblock.
 */
void Expression::RemoveSelfFromBlock()
{
    if (parent != nullptr) {
        parent->RemoveExprOnly(*this);
        parent = nullptr;
    }
    EraseOperands();
}

void Expression::ReplaceWith(Expression& newExpr)
{
    CJC_ASSERT(!newExpr.IsTerminator());
    // 1. replace result
    for (auto user : result->GetUsers()) {
        user->ReplaceOperand(result, newExpr.GetResult());
    }
    // 2. break double linkage between operands and current expr
    EraseOperands();

    // 3. remove new expr from its parent block
    if (newExpr.parent != nullptr) {
        newExpr.parent->RemoveExprOnly(newExpr);
    }

    // 4. replace to new expr in parent block
    CJC_NULLPTR_CHECK(parent);
    for (size_t i = 0; i < parent->exprs.size(); ++i) {
        if (parent->exprs[i] == this) {
            parent->exprs[i] = &newExpr;
        }
    }
    newExpr.parent = parent;
    parent = nullptr;
}

/**
 * @brief Remove this expression from its parent basicblock, and insert it into before the @expr expression.
 *
 * @param expr The destination position which is before this reference expression.
 *
 */
void Expression::MoveBefore(Expression* expr)
{
    CJC_ASSERT(!this->IsTerminator());
    CJC_NULLPTR_CHECK(expr);
    if (this == expr) {
        return;
    }
    // 1. remove current expr from parent block
    if (parent != nullptr) {
        parent->RemoveExprOnly(*this);
    }

    // 2. insert current expr before `expr`
    CJC_NULLPTR_CHECK(expr->parent);
    auto pos = std::find(expr->parent->exprs.begin(), expr->parent->exprs.end(), expr);
    CJC_ASSERT(pos != expr->parent->exprs.end());
    expr->parent->exprs.insert(pos, this);

    // 3. change current expr's parent
    parent = expr->parent;
}

/*
 * @brief Remove this expression from its parent basicblock, and insert it into after the @expr expression.
 *
 * @param expr The destination position which is after this reference expression.
 *
 */
void Expression::MoveAfter(Expression* expr)
{
    CJC_NULLPTR_CHECK(expr);
    // you shouldn't move an expression after a terminator, that's illegal ir
    CJC_ASSERT(!expr->IsTerminator());
    if (parent != nullptr) {
        parent->RemoveExprOnly(*this);
        if (IsTerminator()) {
            for (auto suc : StaticCast<Terminator*>(this)->GetSuccessors()) {
                suc->RemovePredecessor(*parent);
            }
        }
    }
    CJC_NULLPTR_CHECK(expr->parent);
    auto pos = std::find(expr->parent->exprs.begin(), expr->parent->exprs.end(), expr);
    CJC_ASSERT(pos != expr->parent->exprs.end());
    expr->parent->exprs.insert(pos + 1, this);
    parent = expr->parent;
}

void Expression::MoveTo(Block& block)
{
    CJC_NULLPTR_CHECK(parent);
    parent->RemoveExprOnly(*this);
    if (IsTerminator()) {
        for (auto suc : StaticCast<Terminator*>(this)->GetSuccessors()) {
            suc->RemovePredecessor(*parent);
        }
    }
    block.AppendExpression(this);
}

void Expression::SetParent(Block* newParent)
{
    parent = newParent;
}

/*
 * @brief Replace any uses of @from with @to in this expression.
 *
 * @param from The source value.
 *
 * @param to The destination value.
 *
 */
void Expression::ReplaceOperand(Value* oldOperand, Value* newOperand)
{
    CJC_NULLPTR_CHECK(oldOperand);
    CJC_NULLPTR_CHECK(newOperand);
    if (oldOperand == newOperand) {
        return;
    }
    for (unsigned i = 0; i < operands.size(); i++) {
        if (operands[i] == oldOperand) {
            operands[i] = newOperand;
            operands[i]->AddUserOnly(this);
            oldOperand->RemoveUserOnly(this);
        }
    }
}

void Expression::ReplaceOperand(size_t idx, Value* newOperand)
{
    CJC_ASSERT(idx < operands.size());
    auto oldOperand = operands[idx];
    if (oldOperand == newOperand) {
        return;
    }
    operands[idx] = newOperand;
    newOperand->AddUserOnly(this);
    bool needRemoveOldOpUser = true;
    for (auto op : operands) {
        if (op == oldOperand) {
            needRemoveOldOpUser = false;
        }
    }
    if (needRemoveOldOpUser) {
        oldOperand->RemoveUserOnly(this);
    }
}

void Expression::EraseOperands()
{
    for (auto op : operands) {
        op->RemoveUserOnly(this);
    }
    operands.clear();
}

std::string Expression::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    for (unsigned i = 0; i < operands.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << operands[i]->GetIdentifier();
    }
    ss << ")";
    return ss.str();
}

void Expression::Dump() const
{
    std::cout << ToString() << std::endl;
}

void Expression::AppendOperand(Value& op)
{
    operands.emplace_back(&op);
    op.AddUserOnly(this);
}

Value* UnaryExpression::GetOperand() const
{
    return GetOperand(0);
}

Cangjie::OverflowStrategy UnaryExpression::GetOverflowStrategy() const
{
    return overflowStrategy;
}

std::string UnaryExpression::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(" << GetOperand()->GetIdentifier() << ")";
    if (overflowStrategy != Cangjie::OverflowStrategy::NA) {
        ss << " // " << OVERFLOW_TO_STRING_MAP.at(overflowStrategy);
    }
    return ss.str();
}

Value* BinaryExpression::GetLHSOperand() const
{
    return GetOperand(0);
}

Value* BinaryExpression::GetRHSOperand() const
{
    return GetOperand(1);
}

Cangjie::OverflowStrategy BinaryExpression::GetOverflowStrategy() const
{
    return overflowStrategy;
}

std::string BinaryExpression::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(" << GetLHSOperand()->GetIdentifier() << ", " << GetRHSOperand()->GetIdentifier() << ")";
    if (overflowStrategy != Cangjie::OverflowStrategy::NA) {
        ss << " // " << OVERFLOW_TO_STRING_MAP.at(overflowStrategy);
    }
    return ss.str();
}

LiteralValue* Constant::GetValue() const
{
    CJC_ASSERT(GetOperand(0)->IsLiteral());
    return StaticCast<LiteralValue*>(GetOperand(0));
}

bool Constant::IsFuncLit() const
{
    return GetValue()->IsFuncWithBody();
}

const std::vector<GenericType*>& Constant::GetGenericArgs() const
{
    return genericArgs;
}

bool Constant::GetBoolLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsBoolLiteral());
    return static_cast<BoolLiteral*>(val)->GetVal();
}

bool Constant::IsBoolLit() const
{
    return GetValue()->IsBoolLiteral();
}

char32_t Constant::GetRuneLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsRuneLiteral());
    return static_cast<RuneLiteral*>(val)->GetVal();
}

bool Constant::IsRuneLit() const
{
    return GetValue()->IsRuneLiteral();
}

std::string Constant::GetStringLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsStringLiteral());
    return static_cast<StringLiteral*>(val)->GetVal();
}

bool Constant::IsStringLit() const
{
    return GetValue()->IsStringLiteral();
}

int64_t Constant::GetSignedIntLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsIntLiteral());
    return static_cast<IntLiteral*>(val)->GetSignedVal();
}

uint64_t Constant::GetUnsignedIntLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsIntLiteral());
    return static_cast<IntLiteral*>(val)->GetUnsignedVal();
}

bool Constant::IsIntLit() const
{
    return GetValue()->IsIntLiteral();
}

bool Constant::IsSignedIntLit() const
{
    if (!IsIntLit()) {
        return false;
    }
    return static_cast<IntLiteral*>(GetValue())->IsSigned();
}

bool Constant::IsFloatLit() const
{
    return GetValue()->IsFloatLiteral();
}

double Constant::GetFloatLitVal() const
{
    auto val = GetValue();
    CJC_ASSERT(val->IsFloatLiteral());
    return static_cast<FloatLiteral*>(val)->GetVal();
}

bool Constant::IsNullLit() const
{
    return GetValue()->IsNullLiteral();
}

bool Constant::IsUnitLit() const
{
    return GetValue()->IsUnitLiteral();
}

std::string Constant::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "Constant(";
    auto val = GetValue();
    if (auto litVal = dynamic_cast<LiteralValue*>(val)) {
        ss << litVal->ToString();
    } else {
        CJC_ABORT();
    }
    ss << ")";
    return ss.str();
}

// Allocate
Type* Allocate::GetType() const
{
    return ty;
}

std::string Allocate::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "Allocate(" << ty->ToString() << ")";
    return ss.str();
}

Value* Load::GetLocation() const
{
    return operands[0];
}

Value* Store::GetValue() const
{
    return operands[0];
}
Value* Store::GetLocation() const
{
    return operands[1];
}

// GetElementRef
Value* GetElementRef::GetLocation() const
{
    return operands[0];
}

const std::vector<uint64_t>& GetElementRef::GetPath() const
{
    return indexes;
}

std::string GetElementRef::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "GetElementRef(" << operands[0]->GetIdentifier();
    for (auto index : indexes) {
        ss << ", " << index;
    }
    ss << ")";
    return ss.str();
}

Value* StoreElementRef::GetValue() const
{
    return operands[0];
}

Value* StoreElementRef::GetLocation() const
{
    return operands[1];
}

const std::vector<uint64_t>& StoreElementRef::GetPath() const
{
    return indexes;
}

// Apply
Apply::Apply(Value* callee, const std::vector<Value*>& args, Block* parent)
    : Expression(ExprKind::APPLY, {}, {}, parent)
{
    CJC_NULLPTR_CHECK(parent);
    AppendOperand(*callee);
    for (auto op : args) {
        AppendOperand(*op);
    }
}

Value* Apply::GetCallee() const
{
    return operands[0];
}

void Apply::SetSuperCall(bool newIsSuperCall)
{
    this->isSuperCall = newIsSuperCall;
}

bool Apply::IsSuperCall() const
{
    return isSuperCall;
}

void Apply::SetInstantiatedArgTypes(const std::vector<Type*>& args)
{
    instantiateArgs = args;
}

// remove this API after we fully deprecated Ptr<XXX>
void Apply::SetInstantiatedArgTypes(const std::vector<Ptr<Type>>& args)
{
    instantiateArgs.clear();
    for (auto& arg : args) {
        instantiateArgs.emplace_back(arg);
    }
}

void Apply::SetInstantiatedFuncType(
    Type* thisTy, Type* instParentCustomTy, std::vector<Type*> paramTys, Type& retTy)
{
    instFuncType.instParentCustomTy = instParentCustomTy;
    instFuncType.thisType = thisTy;
    instFuncType.instParamTys = std::move(paramTys);
    instFuncType.instRetTy = &retTy;
}

void Apply::SetInstantiatedRetType(Type& retTy)
{
    instFuncType.instRetTy = &retTy;
}

void Apply::SetInstantiatedParamType(size_t idx, Type& paramTy)
{
    CJC_ASSERT(idx < instFuncType.instParamTys.size());
    instFuncType.instParamTys[idx] = &paramTy;
}

Type* Apply::GetThisType() const
{
    return instFuncType.thisType;
}

void Apply::SetThisType(Type& thisType)
{
    instFuncType.thisType = &thisType;
}

Type* Apply::GetInstParentCustomTyOfCallee() const
{
    return instFuncType.instParentCustomTy;
}

void Apply::SetInstParentCustomTyOfCallee(Type& parentType)
{
    instFuncType.instParentCustomTy = &parentType;
}

std::vector<Type*> Apply::GetInstantiatedParamTypes() const
{
    return instFuncType.instParamTys;
}

Type* Apply::GetInstantiatedRetType() const
{
    return instFuncType.instRetTy;
}

const std::vector<Type*>& Apply::GetInstantiateArgs() const
{
    return instantiateArgs;
}

CalleeInfo Apply::GetCalleeTypeInfo() const
{
    return instFuncType;
}

std::vector<Value*> Apply::GetArgs() const
{
    std::vector<Value*> args;
    if (operands.size() > 1) {
        args.assign(operands.begin() + 1, operands.end());
    }
    return args;
}

static std::string StructFuncTypeToString(const Type* thisTy, const std::vector<Type*>& paramTys, Type* retTy)
{
    std::stringstream ss;
    ss << "(";
    if (thisTy != nullptr) {
        ss << thisTy->ToString();
    } else {
        ss << "nullptr";
    }
    ss << ", [";
    for (size_t i = 0; i < paramTys.size(); ++i) {
        ss << paramTys[i]->ToString();
        if (i < paramTys.size() - 1) {
            ss << ", ";
        }
    }
    ss << "], ";
    if (retTy != nullptr) {
        ss << retTy->ToString();
    } else {
        ss << "nullptr";
    }
    ss << ")";
    return ss.str();
}

std::string Apply::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    if (instFuncType.thisType) {
        ss << "ThisType: ";
        ss << instFuncType.thisType->ToString();
        ss << ", ";
    }
    if (instFuncType.instParentCustomTy) {
        ss << instFuncType.instParentCustomTy->ToString();
        ss << "::";
    }
    ss << GetCallee()->GetIdentifier();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instType : instantiateArgs) {
            ss << instType->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << " : (";
    for (size_t i = 0; i < instFuncType.instParamTys.size(); ++i) {
        ss << instFuncType.instParamTys[i]->ToString();
        if (i < instFuncType.instParamTys.size() - 1) {
            ss << ", ";
        }
    }
    ss << ") -> ";
    ss << instFuncType.instRetTy->ToString();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ")";
    if (IsSuperCall()) {
        ss << " // isSuperCall";
    }

    return ss.str();
}

inline static void CheckVirFuncInvokeInfo(const InvokeCalleeInfo& info)
{
    CJC_ASSERT(!info.srcCodeIdentifier.empty());
    CJC_NULLPTR_CHECK(info.instFuncType);
    CJC_NULLPTR_CHECK(info.originalFuncType);
    CJC_NULLPTR_CHECK(info.instParentCustomTy);
    CJC_NULLPTR_CHECK(info.originalParentCustomTy);
    CJC_NULLPTR_CHECK(info.thisType);
}

// Invoke
Invoke::Invoke(Value* object, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo, Block* parent)
    : Expression(ExprKind::INVOKE, {}, {}, parent), funcInfo(funcInfo)
{
    CJC_NULLPTR_CHECK(parent);
    CheckVirFuncInvokeInfo(funcInfo);
    AppendOperand(*object);
    for (auto op : args) {
        AppendOperand(*op);
    }
}

Value* Invoke::GetObject() const
{
    return operands[0];
}

std::string Invoke::GetMethodName() const
{
    return funcInfo.srcCodeIdentifier;
}

FuncType* Invoke::GetMethodType() const
{
    return funcInfo.originalFuncType;
}

ClassType* Invoke::GetInstParentCustomTyOfCallee() const
{
    return funcInfo.instParentCustomTy;
}

std::vector<Type*> Invoke::GetInstantiatedParamTypes() const
{
    return funcInfo.instFuncType->GetParamTypes();
}

Type* Invoke::GetInstantiatedRetType() const
{
    return funcInfo.instFuncType->GetReturnType();
}

void Invoke::SetInstantiatedFuncType(FuncType& funcType)
{
    funcInfo.instFuncType = &funcType;
}

std::vector<Type*> Invoke::GetInstantiatedTypeArgs() const
{
    return funcInfo.instantiatedTypeArgs;
}

void Invoke::SetFuncInfo(const InvokeCalleeInfo& info)
{
    funcInfo = info;
}

InvokeCalleeInfo Invoke::GetFuncInfo() const
{
    return funcInfo;
}

std::string Invoke::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetMethodName();
    auto instantiateArgs = GetInstantiatedTypeArgs();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instType : instantiateArgs) {
            ss << instType->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << ": (";
    // Method type: the return type is not sure.
    auto argTys = GetMethodType()->GetParamTypes();
    for (unsigned i = 0; i < argTys.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << argTys[i]->ToString();
    }
    ss << "), ";

    ss << GetObject()->GetIdentifier();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ")(instParentCustomTy:" << funcInfo.instParentCustomTy->ToString() << ", offset:" << funcInfo.offset << ")";
    return ss.str();
}

std::vector<Value*> Invoke::GetArgs() const
{
    std::vector<Value*> args;
    if (operands.size() > 1) {
        args.assign(operands.begin() + 1, operands.end());
    }
    return args;
}

InvokeStatic::InvokeStatic(
    Value* rtti, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo, Block* parent)
    : Expression(ExprKind::INVOKESTATIC, {}, {}, parent), funcInfo(funcInfo)
{
    CJC_NULLPTR_CHECK(parent);
    CheckVirFuncInvokeInfo(funcInfo);
    AppendOperand(*rtti);
    for (auto op : args) {
        AppendOperand(*op);
    }
}

std::string InvokeStatic::GetMethodName() const
{
    return funcInfo.srcCodeIdentifier;
}

FuncType* InvokeStatic::GetMethodType() const
{
    return funcInfo.originalFuncType;
}

Value* InvokeStatic::GetRTTIValue() const
{
    return operands[0];
}

Type* InvokeStatic::GetThisType() const
{
    return funcInfo.thisType;
}

Type* InvokeStatic::GetInstParentCustomTyOfCallee() const
{
    return funcInfo.instParentCustomTy;
}

std::vector<Type*> InvokeStatic::GetInstantiatedParamTypes() const
{
    return funcInfo.instFuncType->GetParamTypes();
}

Type* InvokeStatic::GetInstantiatedRetType() const
{
    return funcInfo.instFuncType->GetReturnType();
}

void InvokeStatic::SetInstantiatedFuncType(FuncType& funcType)
{
    funcInfo.instFuncType = &funcType;
}

std::vector<Type*> InvokeStatic::GetInstantiatedTypeArgs() const
{
    return funcInfo.instantiatedTypeArgs;
}

void InvokeStatic::SetFuncInfo(const InvokeCalleeInfo& info)
{
    funcInfo = info;
}

InvokeCalleeInfo InvokeStatic::GetFuncInfo() const
{
    return funcInfo;
}

std::string InvokeStatic::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << funcInfo.instParentCustomTy->ToString();
    ss << "::";
    ss << GetMethodName();
    auto instantiateArgs = GetInstantiatedTypeArgs();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instType : instantiateArgs) {
            ss << instType->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << ": (";
    auto argTys = GetMethodType()->GetParamTypes();
    for (unsigned i = 0; i < argTys.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << argTys[i]->ToString();
    }
    ss << ")";

    ss << ", " << GetRTTIValue()->GetIdentifier();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ")" << StructFuncTypeToString(
        funcInfo.thisType, funcInfo.instFuncType->GetParamTypes(), funcInfo.instFuncType->GetReturnType());
    return ss.str();
}

std::vector<Value*> InvokeStatic::GetArgs() const
{
    return {operands.begin() + 1, operands.end()};
}

InvokeStatic* InvokeStatic::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<InvokeStatic>(
        result->GetType(), GetRTTIValue(), GetArgs(), funcInfo, &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// TypeCast
Value* TypeCast::GetSourceValue() const
{
    return operands[0];
}

Type* TypeCast::GetSourceTy() const
{
    return operands[0]->GetType();
}

Type* TypeCast::GetTargetTy() const
{
    return result->GetType();
}

Cangjie::OverflowStrategy TypeCast::GetOverflowStrategy() const
{
    return overflowStrategy;
}

std::string TypeCast::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    Type* targetType = result->GetType();
    ss << "TypeCast(" << operands[0]->GetIdentifier() << ", " << targetType->ToString() << ")";
    if (overflowStrategy != Cangjie::OverflowStrategy::NA) {
        ss << " // " << OVERFLOW_TO_STRING_MAP.at(overflowStrategy);
    }
    return ss.str();
}

// InstanceOf
Value* InstanceOf::GetObject() const
{
    return operands[0];
}

Type* InstanceOf::GetType() const
{
    return ty;
}

std::string InstanceOf::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "InstanceOf(" << operands[0]->GetIdentifier() << ", " << ty->ToString() << ")";
    return ss.str();
}

// Box
Value* Box::GetObject() const
{
    return operands[0];
}

std::string Box::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "Box(" << operands[0]->GetIdentifier() << ", " << GetResult()->GetType()->ToString() << ")";
    return ss.str();
}

Type* Box::GetSourceTy() const
{
    return GetObject()->GetType();
}

Type* Box::GetTargetTy() const
{
    return result->GetType();
}

Box* Box::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Box>(result->GetType(), GetObject(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// UnBox
Value* UnBox::GetObject() const
{
    return operands[0];
}

std::string UnBox::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "UnBox(" << operands[0]->GetIdentifier() << ", " << GetResult()->GetType()->ToString() << ")";
    return ss.str();
}

Type* UnBox::GetSourceTy() const
{
    return GetObject()->GetType();
}

Type* UnBox::GetTargetTy() const
{
    return result->GetType();
}

UnBox* UnBox::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<UnBox>(result->GetType(), GetObject(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// TransformToGeneric
Value* TransformToGeneric::GetObject() const
{
    return operands[0];
}

std::string TransformToGeneric::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "TransformToGeneric(" << operands[0]->GetIdentifier() << ", " << GetResult()->GetType()->ToString() << ")";
    return ss.str();
}

Type* TransformToGeneric::GetSourceTy() const
{
    return GetObject()->GetType();
}

Type* TransformToGeneric::GetTargetTy() const
{
    return result->GetType();
}

TransformToGeneric* TransformToGeneric::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<TransformToGeneric>(result->GetType(), GetObject(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// TransformToConcrete
Value* TransformToConcrete::GetObject() const
{
    return operands[0];
}

std::string TransformToConcrete::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "TransformToConcrete(" << operands[0]->GetIdentifier() << ", " << GetResult()->GetType()->ToString() << ")";
    return ss.str();
}

Type* TransformToConcrete::GetSourceTy() const
{
    return GetObject()->GetType();
}

Type* TransformToConcrete::GetTargetTy() const
{
    return result->GetType();
}

TransformToConcrete* TransformToConcrete::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<TransformToConcrete>(result->GetType(), GetObject(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// UnBoxToRef
Value* UnBoxToRef::GetObject() const
{
    return operands[0];
}

std::string UnBoxToRef::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "UnBoxToRef(" << operands[0]->GetIdentifier() << ", " << GetResult()->GetType()->ToString() << ")";
    return ss.str();
}

Type* UnBoxToRef::GetSourceTy() const
{
    return GetObject()->GetType();
}

Type* UnBoxToRef::GetTargetTy() const
{
    return result->GetType();
}

UnBoxToRef* UnBoxToRef::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<UnBoxToRef>(result->GetType(), GetObject(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// Terminator
Terminator::Terminator(
    ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent)
    : Expression(kind, operands, parent)
{
    CJC_NULLPTR_CHECK(parent);
    for (auto succ : successors) {
        AppendSuccessor(*succ);
    }
}

size_t Terminator::GetFirstSuccessorIndex() const
{
    CJC_ASSERT(GetExprMajorKind() == ExprMajorKind::TERMINATOR);
    if (kind == ExprKind::BRANCH || kind == ExprKind::MULTIBRANCH) {
        return 1;
    }
    if (kind == ExprKind::RAISE_EXCEPTION) {
        return 1;
    }
    if (static_cast<uint64_t>(ExprKind::APPLY_WITH_EXCEPTION) <= static_cast<uint64_t>(kind) &&
        static_cast<uint64_t>(kind) <= static_cast<uint64_t>(ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION)) {
        CJC_ASSERT(operands.size() >= WITH_EXCEPTION_SUCCESSOR_NUM);
        return operands.size() - WITH_EXCEPTION_SUCCESSOR_NUM;
    }
    return 0;
}

std::string Terminator::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    if (IsCompileTimeValue()) {
        ss << "[compileTimeVal] ";
    }
    ss << GetExprKindName();
    ss << "(";
    // Operands
    auto ops = GetOperands();
    for (unsigned i = 0; i < ops.size(); i++) {
        if (i > 0) {
            if (i < ops.size() - 1) {
                ss << ", ";
            }
        }
        ss << ops[i]->GetIdentifier();
    }
    auto succs = GetSuccessors();
    if (ops.size() > 0 && succs.size() > 0) {
        ss << ", ";
    }
    for (unsigned i = 0; i < succs.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << succs[i]->GetIdentifier();
    }
    ss << ")";
    return ss.str();
}

// SuffixString for xxxWithException expression
std::string Terminator::SuffixString() const
{
    std::stringstream ss;
    auto succs = GetSuccessors();
    if (succs.size() > 0) {
        // normal case
        ss << "normal: " << succs[0]->GetIdentifier();
    }
    if (succs.size() > 1) {
        // exception case, exception list
        ss << ", exception ";
        if (succs[1]->IsLandingPadBlock()) {
            ss << GetExceptionsStr(succs[1]->GetExceptions()) << ": ";
        }
        ss << succs[1]->GetIdentifier();
    }
    constexpr size_t rethrowIndex = 2;
    if (succs.size() > rethrowIndex) {
        // rethrow case
        ss << ", rethrow: " << succs[rethrowIndex]->GetIdentifier();
    }
    return ss.str();
}

void Terminator::ReplaceWith(Expression& newTerminator)
{
    CJC_ASSERT(newTerminator.IsTerminator());
    if (result != nullptr) {
        CJC_NULLPTR_CHECK(newTerminator.GetResult());
        for (auto user : result->GetUsers()) {
            user->ReplaceOperand(result, newTerminator.GetResult());
        }
    }
    auto tempParent = parent;
    RemoveSelfFromBlock();
    tempParent->AppendTerminator(StaticCast<Terminator*>(&newTerminator));
}

size_t Terminator::GetNumOfSuccessor() const
{
    return operands.size() - GetFirstSuccessorIndex();
}

Block* Terminator::GetSuccessor(size_t index) const
{
    CJC_ASSERT(operands.size() == 0 || index + GetFirstSuccessorIndex() < operands.size());
    return StaticCast<Block*>(operands[GetFirstSuccessorIndex() + index]);
}

size_t Terminator::GetNumOfOperands() const
{
    return GetFirstSuccessorIndex();
}

std::vector<Value*> Terminator::GetOperands() const
{
    CJC_ASSERT(operands.size() >= GetFirstSuccessorIndex());
    return {operands.begin(), operands.begin() + GetFirstSuccessorIndex()};
}

Value* Terminator::GetOperand(size_t idx) const
{
    CJC_ASSERT(idx < GetFirstSuccessorIndex());
    return operands[idx];
}

const std::vector<Block*> Terminator::GetSuccessors() const
{
    std::vector<Block*> succs;
    for (auto i = GetFirstSuccessorIndex(); i < operands.size(); ++i) {
        succs.emplace_back(StaticCast<Block*>(operands[i]));
    }
    return succs;
}

void Terminator::ReplaceSuccessor(Block& oldSuccessor, Block& newSuccessor)
{
    oldSuccessor.RemovePredecessor(*GetParent());
    newSuccessor.AddPredecessor(GetParent());

    auto it = std::find(operands.begin(), operands.end(), &oldSuccessor);
    CJC_ASSERT(it != operands.end());
    while (it != operands.end()) {
        *it = &newSuccessor;
        it = std::find(std::next(it), operands.end(), &oldSuccessor);
    }
}

void Terminator::ReplaceSuccessor(size_t index, Block& newSuccessor)
{
    CJC_ASSERT(operands.size() == 0 || index + GetFirstSuccessorIndex() < operands.size());
    GetSuccessor(index)->RemovePredecessor(*GetParent());
    newSuccessor.AddPredecessor(GetParent());
    operands[GetFirstSuccessorIndex() + index] = &newSuccessor;
}

void Terminator::LetSuccessorsRemoveCurBlock() const
{
    auto succs = GetSuccessors();
    for (auto suc : succs) {
        suc->RemovePredecessor(*parent);
    }
}

void Terminator::RemoveSelfFromBlock()
{
    LetSuccessorsRemoveCurBlock();
    Expression::RemoveSelfFromBlock();
}

void Terminator::AppendSuccessor(Block& block)
{
    operands.emplace_back(&block);
}

Terminator* Terminator::Clone(CHIRBuilder& builder, Block& parent) const
{
    (void)builder;
    (void)parent;
    CJC_ABORT();
    return nullptr;
}

Block* GoTo::GetDestination() const
{
    return GetSuccessor(0);
}

// MultiBranch

MultiBranch::MultiBranch(Value* cond, Block* defaultBlock, const std::vector<uint64_t>& vals,
    const std::vector<Block*>& succs, Block* parent)
    : Terminator(ExprKind::MULTIBRANCH, {cond}, {}, parent), caseVals(vals)
{
    CJC_NULLPTR_CHECK(cond);
    CJC_NULLPTR_CHECK(defaultBlock);
    CJC_NULLPTR_CHECK(parent);
    /* Note that successors[0] is used to store the default basic block */
    AppendSuccessor(*defaultBlock);
    for (auto b : succs) {
        AppendSuccessor(*b);
    }
}

Value* MultiBranch::GetCondition() const
{
    return operands[0];
}

const std::vector<uint64_t>& MultiBranch::GetCaseVals() const
{
    return caseVals;
}

uint64_t MultiBranch::GetCaseValByIndex(size_t index) const
{
    return caseVals[index];
}

Block* MultiBranch::GetCaseBlockByIndex(size_t index) const
{
    return GetSuccessor(index + 1);
}

Block* MultiBranch::GetDefaultBlock() const
{
    return GetSuccessor(0);
}

std::vector<Block*> MultiBranch::GetNormalBlocks() const
{
    auto succs = GetSuccessors();
    return {succs.begin() + 1, succs.end()};
}

std::string MultiBranch::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetCondition()->GetIdentifier();
    ss << ", ";
    ss << GetSuccessor(0)->GetIdentifier();
    for (size_t i = 1; i < GetNumOfSuccessor(); i++) {
        ss << ", [";
        ss << GetCaseValByIndex(i - 1) << ", ";
        ss << GetSuccessor(i)->GetIdentifier() << "]";
    }
    ss << ")";

    return ss.str();
}

ApplyWithException::ApplyWithException(
    Value* callee, const std::vector<Value*>& args, Block* sucBlock, Block* errBlock, Block* parent)
    : Terminator(ExprKind::APPLY_WITH_EXCEPTION, {}, {}, parent)
{
    CJC_NULLPTR_CHECK(callee);
    CJC_NULLPTR_CHECK(sucBlock);
    CJC_NULLPTR_CHECK(errBlock);
    CJC_NULLPTR_CHECK(parent);
    AppendOperand(*callee);
    for (auto op : args) {
        AppendOperand(*op);
    }

    AppendSuccessor(*sucBlock);
    AppendSuccessor(*errBlock);
}

Value* ApplyWithException::GetCallee() const
{
    return operands[0];
}

/** @brief Get a list of the ApplyWithException operation argument nodes */
std::vector<Value*> ApplyWithException::GetArgs() const
{
    if (GetFirstSuccessorIndex() <= 1) {
        return {};
    } else {
        return {operands.begin() + 1, operands.begin() + GetFirstSuccessorIndex()};
    }
}

Block* ApplyWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* ApplyWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

void ApplyWithException::SetInstantiatedArgTypes(const std::vector<Type*>& args)
{
    instantiateArgs = args;
}

// remove this API after we fully deprecated Ptr<XXX>
void ApplyWithException::SetInstantiatedArgTypes(const std::vector<Ptr<Type>>& args)
{
    instantiateArgs.clear();
    for (auto& arg : args) {
        instantiateArgs.emplace_back(arg);
    }
}

// if callee is not member func, `thisTy` is nullptr
void ApplyWithException::SetInstantiatedFuncType(
    Type* thisTy, Type* instParentCustomTy, const std::vector<Type*>& paramTys, Type& retTy)
{
    instFuncType.instParentCustomTy = instParentCustomTy;
    instFuncType.thisType = thisTy;
    instFuncType.instParamTys = paramTys;
    instFuncType.instRetTy = &retTy;
}

void ApplyWithException::SetInstantiatedRetType(Type& retTy)
{
    instFuncType.instRetTy = &retTy;
}

void ApplyWithException::SetInstantiatedParamType(size_t idx, Type& paramTy)
{
    CJC_ASSERT(idx < instFuncType.instParamTys.size());
    instFuncType.instParamTys[idx] = &paramTy;
}

Type* ApplyWithException::GetThisType() const
{
    return instFuncType.thisType;
}

void ApplyWithException::SetThisType(Type& thisType)
{
    instFuncType.thisType = &thisType;
}

Type* ApplyWithException::GetInstParentCustomTyOfCallee() const
{
    return instFuncType.instParentCustomTy;
}

void ApplyWithException::SetInstParentCustomTyOfCallee(Type& parentType)
{
    instFuncType.instParentCustomTy = &parentType;
}

std::vector<Type*> ApplyWithException::GetInstantiatedParamTypes() const
{
    return instFuncType.instParamTys;
}

Type* ApplyWithException::GetInstantiatedRetType() const
{
    return instFuncType.instRetTy;
}

const std::vector<Type*>& ApplyWithException::GetInstantiateArgs() const
{
    return instantiateArgs;
}

CalleeInfo ApplyWithException::GetCalleeTypeInfo() const
{
    return instFuncType;
}

std::string ApplyWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    if (instFuncType.instParentCustomTy) {
        ss << instFuncType.instParentCustomTy->ToString();
        ss << "::";
    }
    ss << GetCallee()->GetIdentifier();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instantiateArg : instantiateArgs) {
            ss << instantiateArg->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << " : (";
    for (size_t i = 0; i < instFuncType.instParamTys.size(); ++i) {
        ss << instFuncType.instParamTys[i]->ToString();
        if (i < instFuncType.instParamTys.size() - 1) {
            ss << ", ";
        }
    }
    ss << ") -> ";
    ss << instFuncType.instRetTy->ToString();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ", " << SuffixString() << ")";
    return ss.str();
}

InvokeWithException::InvokeWithException(Value* object, const std::vector<Value*>& args,
    const InvokeCalleeInfo& funcInfo, Block* sucBlock, Block* errBlock, Block* parent)
    : Terminator(ExprKind::INVOKE_WITH_EXCEPTION, {}, {}, parent), funcInfo(funcInfo)
{
    CJC_NULLPTR_CHECK(object);
    CJC_NULLPTR_CHECK(sucBlock);
    CJC_NULLPTR_CHECK(errBlock);
    CJC_NULLPTR_CHECK(parent);
    CheckVirFuncInvokeInfo(funcInfo);
    AppendOperand(*object);
    for (auto op : args) {
        AppendOperand(*op);
    }

    AppendSuccessor(*sucBlock);
    AppendSuccessor(*errBlock);
}

Value* InvokeWithException::GetObject() const
{
    return operands[0];
}

/** @brief Get the method name of this Invoke operation */
std::string InvokeWithException::GetMethodName() const
{
    return funcInfo.srcCodeIdentifier;
}

/** @brief Get the method type of this Invoke operation */
FuncType* InvokeWithException::GetMethodType() const
{
    return funcInfo.originalFuncType;
}

Type* InvokeWithException::GetInstParentCustomTyOfCallee() const
{
    return funcInfo.instParentCustomTy;
}

std::vector<Type*> InvokeWithException::GetInstantiatedParamTypes() const
{
    return funcInfo.instFuncType->GetParamTypes();
}

Type* InvokeWithException::GetInstantiatedRetType() const
{
    return funcInfo.instFuncType->GetReturnType();
}

void InvokeWithException::SetInstantiatedFuncType(FuncType& funcType)
{
    funcInfo.instFuncType = &funcType;
}

std::vector<Type*> InvokeWithException::GetInstantiatedTypeArgs() const
{
    return funcInfo.instantiatedTypeArgs;
}

void InvokeWithException::SetFuncInfo(const InvokeCalleeInfo& info)
{
    funcInfo = info;
}

InvokeCalleeInfo InvokeWithException::GetFuncInfo() const
{
    return funcInfo;
}

/** @brief Get the call args of this InvokeWithException operation */
std::vector<Value*> InvokeWithException::GetArgs() const
{
    if (GetFirstSuccessorIndex() <= 1) {
        return {};
    } else {
        return {operands.begin() + 1, operands.begin() + GetFirstSuccessorIndex()};
    }
}

Block* InvokeWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* InvokeWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

std::string InvokeWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetMethodName();
    auto instantiateArgs = GetInstantiatedTypeArgs();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instType : instantiateArgs) {
            ss << instType->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << ", ";
    ss << GetMethodType()->ToString();
    ss << ", ";
    ss << GetObject()->GetIdentifier();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ", " << SuffixString();
    ss << ")(instParentCustomTy:" << funcInfo.instParentCustomTy->ToString() << ", offset:" << funcInfo.offset << ")";
    return ss.str();
}

InvokeStaticWithException::InvokeStaticWithException(Value* rtti,
    const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo, Block* sucBlock, Block* errBlock, Block* parent)
    : Terminator(ExprKind::INVOKESTATIC_WITH_EXCEPTION, {}, {}, parent), funcInfo(funcInfo)
{
    AppendOperand(*rtti);
    for (auto arg : args) {
        AppendOperand(*arg);
    }
    AppendSuccessor(*sucBlock);
    AppendSuccessor(*errBlock);
    CJC_NULLPTR_CHECK(sucBlock);
    CJC_NULLPTR_CHECK(errBlock);
    CJC_NULLPTR_CHECK(parent);
    CheckVirFuncInvokeInfo(funcInfo);
}

std::string InvokeStaticWithException::GetMethodName() const
{
    return funcInfo.srcCodeIdentifier;
}

/** @brief Get the method type of this Invoke operation */
FuncType* InvokeStaticWithException::GetMethodType() const
{
    return funcInfo.originalFuncType;
}

Type* InvokeStaticWithException::GetThisType() const
{
    return funcInfo.thisType;
}

Type* InvokeStaticWithException::GetInstParentCustomTyOfCallee() const
{
    return funcInfo.instParentCustomTy;
}

std::vector<Type*> InvokeStaticWithException::GetInstantiatedParamTypes() const
{
    return funcInfo.instFuncType->GetParamTypes();
}

Type* InvokeStaticWithException::GetInstantiatedRetType() const
{
    return funcInfo.instFuncType->GetReturnType();
}

void InvokeStaticWithException::SetInstantiatedFuncType(FuncType& funcType)
{
    funcInfo.instFuncType = &funcType;
}

std::vector<Type*> InvokeStaticWithException::GetInstantiatedTypeArgs() const
{
    return funcInfo.instantiatedTypeArgs;
}

void InvokeStaticWithException::SetFuncInfo(const InvokeCalleeInfo& info)
{
    funcInfo = info;
}

InvokeCalleeInfo InvokeStaticWithException::GetFuncInfo() const
{
    return funcInfo;
}

Value* InvokeStaticWithException::GetRTTIValue() const
{
    return operands[0];
}

Block* InvokeStaticWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* InvokeStaticWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

std::vector<Value*> InvokeStaticWithException::GetArgs() const
{
    return {operands.begin() + 1, operands.begin() + GetFirstSuccessorIndex()};
}

std::string InvokeStaticWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << funcInfo.thisType->ToString();
    ss << "::";
    ss << GetMethodName();
    auto instantiateArgs = GetInstantiatedTypeArgs();
    if (!instantiateArgs.empty()) {
        ss << "<";
        size_t i = 0;
        for (auto instType : instantiateArgs) {
            ss << instType->ToString();
            if (i < instantiateArgs.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
        ss << ">";
    }
    ss << " : (";
    auto argTys = GetMethodType()->GetParamTypes();
    for (unsigned i = 0; i < argTys.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << argTys[i]->ToString();
    }
    ss << ")";

    ss << ", " << GetRTTIValue()->GetIdentifier();
    for (auto id : GetArgs()) {
        ss << ", ";
        ss << id->GetIdentifier();
    }
    ss << ", " << SuffixString() << ")";
    ss << StructFuncTypeToString(
        funcInfo.thisType, funcInfo.instFuncType->GetParamTypes(), funcInfo.instFuncType->GetReturnType());
    return ss.str();
}

// IntOpWithException
ExprKind IntOpWithException::GetOpKind() const
{
    return opKind;
}

Value* IntOpWithException::GetLHSOperand() const
{
    return GetOperand(0);
}

Value* IntOpWithException::GetRHSOperand() const
{
    return GetOperand(1);
}

Block* IntOpWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* IntOpWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

Cangjie::OverflowStrategy IntOpWithException::GetOverflowStrategy() const
{
    if (opKind == ExprKind::DIV) {
        return overflowStrategy;
    }
    return Cangjie::OverflowStrategy::THROWING;
}

std::string IntOpWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << ExprKindMgr::Instance()->GetKindName(static_cast<size_t>(opKind)) << "WithException";
    ss << "(";
    auto ops = GetOperands();
    ss << ops[0]->GetIdentifier();
    if (ops.size() > 1) {
        ss << ", " << ops[1]->GetIdentifier();
    }
    ss << ", " << SuffixString() << ")";
    return ss.str();
}

// TypeCastWithException
Cangjie::OverflowStrategy TypeCastWithException::GetOverflowStrategy() const
{
    return Cangjie::OverflowStrategy::THROWING;
}

/** @brief Get the source value of this cast operation */
Value* TypeCastWithException::GetSourceValue() const
{
    return operands[0];
}

Block* TypeCastWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* TypeCastWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

Type* TypeCastWithException::GetSourceTy() const
{
    return operands[0]->GetType();
}

Type* TypeCastWithException::GetTargetTy() const
{
    return result->GetType();
}

std::string TypeCastWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetSourceValue()->GetIdentifier();
    ss << ", ";
    ss << GetTargetTy()->ToString();
    ss << ", " << SuffixString() << ")";
    return ss.str();
}

// IntrinsicWithException
IntrinsicKind IntrinsicWithException::GetIntrinsicKind() const
{
    return intrinsicKind;
}

const std::vector<Ptr<Type>>& IntrinsicWithException::GetGenericTypeInfo() const
{
    return genericTypeInfo;
}

void IntrinsicWithException::SetGenericTypeInfo(const std::vector<Ptr<Type>>& types)
{
    genericTypeInfo = types;
}

Block* IntrinsicWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* IntrinsicWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

const std::vector<Value*> IntrinsicWithException::GetArgs() const
{
    return GetOperands();
}

std::string IntrinsicWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName() << "/" << INTRINSIC_KIND_TO_STRING_MAP.at(intrinsicKind);
    ss << "(";
    auto ops = GetOperands();
    for (auto op : ops) {
        ss << op->GetIdentifier() << ", ";
    }
    ss << SuffixString() << ")";
    if (!genericTypeInfo.empty()) {
        ss << " // genericTypeInfo: " << genericTypeInfo[0]->ToString();
        for (size_t loop = 1; loop < genericTypeInfo.size(); loop++) {
            ss << ", " << genericTypeInfo[loop]->ToString();
        }
    }
    return ss.str();
}

// AllocateWithException
Type* AllocateWithException::GetType() const
{
    return ty;
}

Block* AllocateWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* AllocateWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

std::string AllocateWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << ty->ToString();
    ss << ", " << SuffixString() << ")";
    return ss.str();
}

// RawArrayAllocateWithException
Value* RawArrayAllocateWithException::GetSize() const
{
    return operands[0];
}

Type* RawArrayAllocateWithException::GetElementType() const
{
    return elementType;
}

void RawArrayAllocateWithException::SetElementType(Type& type)
{
    elementType = &type;
}

Block* RawArrayAllocateWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* RawArrayAllocateWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

std::string RawArrayAllocateWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << elementType->ToString();
    ss << ", ";
    ss << GetSize()->GetIdentifier();
    ss << ", " << SuffixString() << ")";
    return ss.str();
}

Value* SpawnWithException::GetFuture() const
{
    CJC_ASSERT(!isExecuteClosure);
    return operands[0];
}

std::optional<Value*> SpawnWithException::GetSpawnArg() const
{
    if (GetFirstSuccessorIndex() > 1) {
        return operands[1];
    }
    return std::nullopt;
}

Value* SpawnWithException::GetClosure() const
{
    CJC_ASSERT(isExecuteClosure);
    return operands[0];
}

FuncBase* SpawnWithException::GetExecuteClosure() const
{
    CJC_ASSERT(isExecuteClosure);
    return executeClosure;
}

bool SpawnWithException::IsExecuteClosure() const
{
    return isExecuteClosure;
}

Block* SpawnWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* SpawnWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

std::string SpawnWithException::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    for (size_t i = 0; i < GetFirstSuccessorIndex(); i++) {
        ss << operands[i]->GetIdentifier() << ", ";
    }
    ss << SuffixString() << ")";
    if (IsExecuteClosure()) {
        ss << " // executeClosure: " + GetExecuteClosure()->GetIdentifier();
    }
    return ss.str();
}

SpawnWithException* SpawnWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    SpawnWithException* newNode = nullptr;
    auto arg = GetSpawnArg();
    if (arg.has_value()) {
        newNode = builder.CreateExpression<SpawnWithException>(result->GetType(), GetFuture(), arg.value(),
            executeClosure, IsExecuteClosure(), GetSuccessBlock(), GetErrorBlock(), &parent);
    } else {
        newNode = builder.CreateExpression<SpawnWithException>(result->GetType(), GetFuture(), executeClosure,
            IsExecuteClosure(), GetSuccessBlock(), GetErrorBlock(), &parent);
    }
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

// Tuple
std::string Tuple::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    for (unsigned i = 0; i < operands.size(); i++) {
        ss << operands[i]->GetIdentifier();
        if (i < operands.size() - 1) {
            ss << ", ";
        }
    }
    ss << ")";
    return ss.str();
}

// Field
Value* Field::GetBase() const
{
    return operands[0];
}

std::vector<uint64_t> Field::GetIndexes() const
{
    return indexes;
}

std::string Field::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetBase()->GetIdentifier();
    for (auto ind : GetIndexes()) {
        ss << ", " << ind;
    }
    ss << ")";
    return ss.str();
}

// RawArrayAllocate
Value* RawArrayAllocate::GetSize() const
{
    return operands[0];
}

Type* RawArrayAllocate::GetElementType() const
{
    return elementType;
}

void RawArrayAllocate::SetElementType(Type& type)
{
    elementType = &type;
}

std::string RawArrayAllocate::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << elementType->ToString();
    ss << ", ";
    ss << GetSize()->GetIdentifier();
    ss << ")";
    return ss.str();
}

// RawArrayLiteralInit
RawArrayLiteralInit::RawArrayLiteralInit(const Ptr<Value> raw, std::vector<Value*> elements, Block* parent)
    : Expression(ExprKind::RAW_ARRAY_LITERAL_INIT, {}, {}, parent)
{
    CJC_NULLPTR_CHECK(parent);
    AppendOperand(*raw);
    for (auto op : elements) {
        AppendOperand(*op);
    }
}

Value* RawArrayLiteralInit::GetRawArray() const
{
    return operands[0];
}

size_t RawArrayLiteralInit::GetSize() const
{
    return operands.size() - 1;
}

std::vector<Value*> RawArrayLiteralInit::GetElements() const
{
    if (operands.size() > 0) {
        return {operands.begin() + 1, operands.end()};
    }
    return {};
}

// Intrinsic
IntrinsicKind Intrinsic::GetIntrinsicKind() const
{
    return intrinsicKind;
}

const std::vector<Ptr<Type>>& Intrinsic::GetGenericTypeInfo() const
{
    return genericTypeInfo;
}

void Intrinsic::SetGenericTypeInfo(const std::vector<Ptr<Type>>& types)
{
    genericTypeInfo = types;
}

const std::vector<Value*>& Intrinsic::GetArgs() const
{
    return operands;
}

std::string Intrinsic::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName() << "/" << INTRINSIC_KIND_TO_STRING_MAP.at(intrinsicKind);
    ss << "(";
    for (size_t i = 0; i < operands.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << operands[i]->GetIdentifier();
    }
    ss << ")";
    if (!genericTypeInfo.empty()) {
        ss << " // genericTypeInfo: " << genericTypeInfo[0]->ToString();
        for (size_t loop = 1; loop < genericTypeInfo.size(); loop++) {
            ss << ", " << genericTypeInfo[loop]->ToString();
        }
    }
    return ss.str();
}

// If
Value* If::GetCondition() const
{
    return operands[0];
}

BlockGroup* If::GetTrueBranch() const
{
    return blockGroups[0];
}

BlockGroup* If::GetFalseBranch() const
{
    return blockGroups[1];
}

std::string If::ToString(unsigned indent) const
{
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(" << operands[0]->GetIdentifier() << ", " << std::endl;
    ss << GetBlockGroupStr(*blockGroups[0], indent + 1);
    ss << "," << std::endl;
    ss << GetBlockGroupStr(*blockGroups[1], indent + 1);
    ss << ")";
    return ss.str();
}

// Loop
BlockGroup* Loop::GetLoopBody() const
{
    return blockGroups[0];
}

std::string Loop::ToString(unsigned indent) const
{
    std::stringstream ss;
    ss << GetExprKindName() << "(" << std::endl;
    ss << GetBlockGroupStr(*blockGroups[0], indent + 1);
    ss << ")";
    return ss.str();
}

// ForIn
Value* ForIn::GetInductionVar() const
{
    return operands[0];
}

Value* ForIn::GetLoopCondVar() const
{
    return operands[1];
}

BlockGroup* ForIn::GetBody() const
{
    return blockGroups[0];
}

BlockGroup* ForIn::GetLatch() const
{
    return blockGroups[1];
}

BlockGroup* ForIn::GetCond() const
{
    return blockGroups[2U];
}

std::string ForIn::ToString(unsigned indent) const
{
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetInductionVar()->GetIdentifier();
    ss << ", " << GetLoopCondVar()->GetIdentifier() << "," << std::endl;
    ss << GetBlockGroupStr(*GetBody(), indent + 1);
    ss << "," << std::endl;
    ss << GetBlockGroupStr(*GetLatch(), indent + 1);
    ss << "," << std::endl;
    ss << GetBlockGroupStr(*GetCond(), indent + 1);
    ss << ")";
    return ss.str();
}

// Lambda
Lambda::Lambda(FuncType* ty, Block* parent, bool isLocalFunc, const std::string& identifier,
    const std::string& srcCodeIdentifier, const std::vector<GenericType*>& genericTypeParams)
    : Expression(ExprKind::LAMBDA, /* operands = */ {}, /* block group = */ {}, parent),
      identifier(identifier),
      srcCodeIdentifier(srcCodeIdentifier),
      funcTy(ty),
      isLocalFunc(isLocalFunc),
      genericTypeParams(genericTypeParams)
{
    CJC_NULLPTR_CHECK(ty);
    CJC_NULLPTR_CHECK(parent);
}

Type* Lambda::GetReturnType() const
{
    return funcTy->GetReturnType();
}

std::string Lambda::ToString(unsigned indent) const
{
    return GetLambdaStr(*this, indent);
}

std::vector<Value*> Lambda::GetEnvironmentVars() const
{
    std::vector<Value*> envs;
    auto preVisit = [&envs, this](Expression& e) {
        for (auto op : e.GetOperands()) {
            bool isEnv = false;
            if (op->IsLocalVar()) {
                auto localVar = static_cast<LocalVar*>(op);
                if (localVar != GetResult() && localVar->GetOwnerBlockGroup() != GetBody()) {
                    isEnv = true;
                }
            } else if (op->IsParameter()) {
                auto arg = static_cast<Parameter*>(op);
                if (arg->GetOwnerLambda() != this) {
                    isEnv = true;
                }
            }
            if (isEnv && std::find(envs.begin(), envs.end(), op) == envs.end()) {
                envs.emplace_back(op);
            }
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(*GetBody(), preVisit, [](Expression&) { return VisitResult::CONTINUE; });
    return envs;
}

// Debug
Debug::Debug(Value* local, std::string srcCodeIdentifier, Block* parent)
    : Expression(ExprKind::DEBUGEXPR, {local}, parent), srcCodeIdentifier(srcCodeIdentifier)
{
    CJC_NULLPTR_CHECK(parent);
}

std::string Debug::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << operands[0]->GetIdentifier();
    ss << ", " << srcCodeIdentifier << ")";
    return ss.str();
}

Value* Spawn::GetFuture() const
{
    CJC_ASSERT(!isExecuteClosure);
    return operands[0];
}

std::optional<Value*> Spawn::GetSpawnArg() const
{
    if (operands.size() > 1) {
        return operands[1];
    }
    return std::nullopt;
}

Value* Spawn::GetClosure() const
{
    CJC_ASSERT(isExecuteClosure);
    return operands[0];
}

FuncBase* Spawn::GetExecuteClosure() const
{
    CJC_ASSERT(isExecuteClosure);
    return executeClosure;
}

bool Spawn::IsExecuteClosure() const
{
    return isExecuteClosure;
}

void Spawn::SetExecuteClosure(bool newIsExecuteClosure)
{
    isExecuteClosure = newIsExecuteClosure;
}

std::string Spawn::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << Expression::ToString(indent);
    if (IsExecuteClosure()) {
        ss << " // executeClosure: " + GetExecuteClosure()->GetIdentifier();
    }
    return ss.str();
}

UnaryExpression* UnaryExpression::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<UnaryExpression>(
        result->GetType(), GetExprKind(), GetOperand(), GetOverflowStrategy(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

BinaryExpression* BinaryExpression::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<BinaryExpression>(
        result->GetType(), GetExprKind(), GetLHSOperand(), GetRHSOperand(), GetOverflowStrategy(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Constant* Constant::Clone(CHIRBuilder& builder, Block& parent) const
{
    Constant* newNode = nullptr;
    auto litVal = StaticCast<LiteralValue*>(GetValue());
    if (litVal->IsNullLiteral()) {
        litVal = builder.CreateLiteralValue<NullLiteral>(litVal->GetType());
    }
    newNode = builder.CreateExpression<Constant>(result->GetType(), litVal, &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Allocate* Allocate::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Allocate>(result->GetType(), GetType(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Load* Load::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Load>(result->GetType(), GetLocation(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Store* Store::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Store>(result->GetType(), GetValue(), GetLocation(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

GetElementRef* GetElementRef::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<GetElementRef>(result->GetType(), GetLocation(), GetPath(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

std::string StoreElementRef::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << "StoreElementRef(" << operands[0]->GetIdentifier() << ", " << operands[1]->GetIdentifier();
    for (auto index : indexes) {
        ss << ", " << index;
    }
    ss << ")";
    return ss.str();
}

StoreElementRef* StoreElementRef::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode =
        builder.CreateExpression<StoreElementRef>(result->GetType(), GetValue(), GetLocation(), GetPath(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Apply* Apply::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Apply>(
        result->GetType(), GetCallee(), GetArgs(), &parent);
    newNode->SetInstantiatedArgTypes(GetInstantiateArgs());
    newNode->SetInstantiatedFuncType(
        GetThisType(), GetInstParentCustomTyOfCallee(), GetInstantiatedParamTypes(), *GetInstantiatedRetType());
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Invoke* Invoke::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Invoke>(
        result->GetType(), GetObject(), GetArgs(), funcInfo, &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

TypeCast* TypeCast::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode =
        builder.CreateExpression<TypeCast>(result->GetType(), GetSourceValue(), GetOverflowStrategy(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    // Need to keep the NeedCheckCast for TypeCast Clone
    newNode->Set<NeedCheckCast>(this->Get<NeedCheckCast>());
    newNode->GetResult()->Set<EnumCaseIndex>(result->Get<EnumCaseIndex>());
    return newNode;
}

InstanceOf* InstanceOf::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<InstanceOf>(result->GetType(), GetObject(), GetType(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

GoTo* GoTo::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<GoTo>(GetDestination(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

Value* Branch::GetCondition() const
{
    return operands[0];
}

Block* Branch::GetTrueBlock() const
{
    return GetSuccessor(0);
}

Block* Branch::GetFalseBlock() const
{
    return GetSuccessor(1);
}

std::string Branch::ToString(unsigned indent) const
{
    const static std::unordered_map<SourceExpr, std::string> SOURCE_EXPR_MAP = {
        {SourceExpr::IF_EXPR, "IF_EXPR"},
        {SourceExpr::WHILE_EXPR, "WHILE_EXPR"},
        {SourceExpr::DO_WHILE_EXPR, "DO_WHILE_EXPR"},
        {SourceExpr::MATCH_EXPR, "MATCH_EXPR"},
        {SourceExpr::IF_LET_OR_WHILE_LET, "IF_LET_OR_WHILE_LET"},
        {SourceExpr::QUEST, "QUEST"},
        {SourceExpr::BINARY, "BINARY"},
        {SourceExpr::FOR_IN_EXPR, "FOR_IN_EXPR"},
        {SourceExpr::OTHER, "OTHER"},
    };
    std::stringstream ss;
    ss << Expression::ToString(indent);
    ss << " // sourceExpr: " << SOURCE_EXPR_MAP.at(sourceExpr);
    return ss.str();
}

Value* RaiseException::GetExceptionValue() const
{
    return operands[0];
}

Block* RaiseException::GetExceptionBlock() const
{
    auto succs = GetSuccessors();
    if (!succs.empty()) {
        return succs[0];
    }
    return nullptr;
}

Value* RawArrayInitByValue::GetRawArray() const
{
    return operands[0];
}

Value* RawArrayInitByValue::GetSize() const
{
    return operands[1];
}

Value* RawArrayInitByValue::GetInitValue() const
{
    constexpr int initIdx = 2;
    return operands[initIdx];
}

int64_t VArray::GetSize() const
{
    return static_cast<int64_t>(operands.size());
}

Value* VArrayBuilder::GetSize() const
{
    return GetOperand(static_cast<size_t>(ElementIdx::SIZE_IDX));
}

Value* VArrayBuilder::GetItem() const
{
    return GetOperand(static_cast<size_t>(ElementIdx::ITEM_IDX));
}

Value* VArrayBuilder::GetInitFunc() const
{
    return GetOperand(static_cast<size_t>(ElementIdx::INIT_FUNC_IDX));
}

ForIn::BGExecutionOrder ForInRange::GetExecutionOrder() const
{
    return {GetCond(), GetBody(), GetLatch()};
}

ForIn::BGExecutionOrder ForInIter::GetExecutionOrder() const
{
    return {GetLatch(), GetCond(), GetBody()};
}

ForIn::BGExecutionOrder ForInClosedRange::GetExecutionOrder() const
{
    return {GetBody(), GetCond(), GetLatch()};
}

Branch* Branch::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<Branch>(GetCondition(), GetTrueBlock(), GetFalseBlock(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

MultiBranch* MultiBranch::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<MultiBranch>(
        GetCondition(), GetDefaultBlock(), GetCaseVals(), GetNormalBlocks(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

Exit* Exit::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<Exit>(&parent);
    parent.AppendExpression(newNode);
    return newNode;
}

RaiseException* RaiseException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto excp = GetExceptionBlock();
    auto newNode = (excp ? builder.CreateTerminator<RaiseException>(GetExceptionValue(), excp, &parent)
                         : builder.CreateTerminator<RaiseException>(GetExceptionValue(), &parent));
    parent.AppendExpression(newNode);
    return newNode;
}

ApplyWithException* ApplyWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    ApplyWithException* newNode = builder.CreateExpression<ApplyWithException>(result->GetType(), GetCallee(),
        GetArgs(), GetSuccessBlock(), GetErrorBlock(), &parent);
    newNode->SetInstantiatedArgTypes(GetInstantiateArgs());
    newNode->SetInstantiatedFuncType(
        GetThisType(), GetInstParentCustomTyOfCallee(), GetInstantiatedParamTypes(), *GetInstantiatedRetType());
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

InvokeWithException* InvokeWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    InvokeWithException* newNode = builder.CreateExpression<InvokeWithException>(
        result->GetType(), GetObject(), GetArgs(), funcInfo, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

InvokeStaticWithException* InvokeStaticWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    InvokeStaticWithException* newNode = builder.CreateExpression<InvokeStaticWithException>(
        result->GetType(), GetRTTIValue(), GetArgs(), funcInfo, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

IntOpWithException* IntOpWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    IntOpWithException* newNode = nullptr;
    if (GetFirstSuccessorIndex() == 1) {
        newNode = builder.CreateExpression<IntOpWithException>(
            result->GetType(), GetOpKind(), GetLHSOperand(), GetSuccessBlock(), GetErrorBlock(), &parent);
    } else {
        newNode = builder.CreateExpression<IntOpWithException>(result->GetType(), GetOpKind(), GetLHSOperand(),
            GetRHSOperand(), GetSuccessBlock(), GetErrorBlock(), &parent);
    }
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

TypeCastWithException* TypeCastWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<TypeCastWithException>(
        result->GetType(), GetSourceValue(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

IntrinsicWithException* IntrinsicWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<IntrinsicWithException>(
        result->GetType(), GetIntrinsicKind(), GetArgs(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->SetGenericTypeInfo(GetGenericTypeInfo());
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

AllocateWithException* AllocateWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<AllocateWithException>(
        result->GetType(), GetType(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

RawArrayAllocateWithException* RawArrayAllocateWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<RawArrayAllocateWithException>(
        result->GetType(), GetElementType(), GetSize(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Tuple* Tuple::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Tuple>(result->GetType(), GetOperands(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Field* Field::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Field>(result->GetType(), GetBase(), GetIndexes(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

RawArrayAllocate* RawArrayAllocate::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<RawArrayAllocate>(result->GetType(), GetElementType(), GetSize(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

RawArrayLiteralInit* RawArrayLiteralInit::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode =
        builder.CreateExpression<RawArrayLiteralInit>(result->GetType(), GetRawArray(), GetElements(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

RawArrayInitByValue* RawArrayInitByValue::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<RawArrayInitByValue>(
        result->GetType(), GetRawArray(), GetSize(), GetInitValue(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

VArray* VArray::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<VArray>(result->GetType(), GetOperands(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

VArrayBuilder* VArrayBuilder::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode =
        builder.CreateExpression<VArrayBuilder>(result->GetType(), GetSize(), GetItem(), GetInitFunc(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

GetException* GetException::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<GetException>(result->GetType(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Intrinsic* Intrinsic::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Intrinsic>(result->GetType(), GetIntrinsicKind(), GetArgs(), &parent);
    parent.AppendExpression(newNode);
    newNode->SetGenericTypeInfo(GetGenericTypeInfo());
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

If* If::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode =
        builder.CreateExpression<If>(result->GetType(), GetCondition(), GetTrueBranch(), GetFalseBranch(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Loop* Loop::Clone(CHIRBuilder& builder, Block& parent) const
{
    BlockGroup* newBody = nullptr;
    if (parent.GetParentBlockGroup()->GetOwnerFunc()) {
        newBody = GetLoopBody()->Clone(builder, *parent.GetParentBlockGroup()->GetOwnerFunc());
    } else {
        auto parentLambda = StaticCast<Lambda*>(parent.GetParentBlockGroup()->GetOwnerExpression());
        newBody = GetLoopBody()->Clone(builder, *parentLambda);
    }
    auto newNode = builder.CreateExpression<Loop>(result->GetType(), newBody, &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

template <class T> T* ForIn::CloneBase(CHIRBuilder& builder, Block& parent) const
{
    static_assert(std::is_base_of_v<ForIn, T>);
    BlockGroup* newCond = nullptr;
    BlockGroup* newBody = nullptr;
    BlockGroup* newLatch = nullptr;
    if (parent.GetParentBlockGroup()->GetOwnerFunc()) {
        newCond = GetCond()->Clone(builder, *parent.GetParentBlockGroup()->GetOwnerFunc());
        newBody = GetBody()->Clone(builder, *parent.GetParentBlockGroup()->GetOwnerFunc());
        newLatch = GetLatch()->Clone(builder, *parent.GetParentBlockGroup()->GetOwnerFunc());
    } else {
        auto parentLambda = StaticCast<Lambda*>(parent.GetParentBlockGroup()->GetOwnerExpression());
        newCond = GetCond()->Clone(builder, *parentLambda);
        newBody = GetBody()->Clone(builder, *parentLambda);
        newLatch = GetLatch()->Clone(builder, *parentLambda);
    }
    T* newNode = builder.CreateExpression<T>(
        result->GetType(), GetInductionVar(), GetLoopCondVar(), newBody, newLatch, newCond, &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

ForInRange* ForInRange::Clone(CHIRBuilder &builder, Block &parent) const
{
    return CloneBase<ForInRange>(builder, parent);
}

ForInIter* ForInIter::Clone(CHIRBuilder &builder, Block &parent) const
{
    return CloneBase<ForInIter>(builder, parent);
}

ForInClosedRange* ForInClosedRange::Clone(CHIRBuilder& builder, Block& parent) const
{
    return CloneBase<ForInClosedRange>(builder, parent);
}

static std::pair<size_t, size_t> GetAllocExprOfRetVal(const BlockGroup& body)
{
    auto blocks = body.GetBlocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        auto exprs = blocks[i]->GetExpressions();
        for (size_t j = 0; j < exprs.size(); ++j) {
            auto res = exprs[j]->GetResult();
            if (res != nullptr && res->IsRetValue()) {
                return {i, j};
            }
        }
    }
    CJC_ABORT();
    return {0, 0};
}

static uint64_t g_lambdaClonedIdx = 0;

Lambda* Lambda::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newIdentifier = identifier + "." + std::to_string(g_lambdaClonedIdx++);
    auto newNode = builder.CreateExpression<Lambda>(result->GetType(), funcTy, &parent, IsLocalFunc(), newIdentifier,
        GetSrcCodeIdentifier(), GetGenericTypeParams());
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    auto newBody = GetLambdaBody()->Clone(builder, *newNode);
    for (auto p : GetParams()) {
        builder.CreateParameter(p->GetType(), INVALID_LOCATION, *newNode);
    }
    auto [blockIdx, exprIdx] = GetAllocExprOfRetVal(*GetLambdaBody());
    auto newRet = newBody->GetBlockByIdx(blockIdx)->GetExpressionByIdx(exprIdx)->GetResult();
    CJC_NULLPTR_CHECK(newRet);
    newNode->SetReturnValue(*newRet);
    newNode->SetCapturedVars(capturedVars);
    return newNode;
}

Debug* Debug::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<Debug>(result->GetType(), GetValue(), GetSrcCodeIdentifier(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

Spawn* Spawn::Clone(CHIRBuilder& builder, Block& parent) const
{
    Spawn* newNode = nullptr;
    auto arg = GetSpawnArg();
    if (arg.has_value()) {
        newNode = builder.CreateExpression<Spawn>(
            result->GetType(), GetFuture(), arg.value(), executeClosure, IsExecuteClosure(), &parent);
    } else {
        newNode = builder.CreateExpression<Spawn>(
            result->GetType(), GetFuture(), executeClosure, IsExecuteClosure(), &parent);
    }
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

GetInstantiateValue* GetInstantiateValue::Clone(CHIRBuilder& builder, Block& parent) const
{
    GetInstantiateValue* newNode = nullptr;
    auto val = GetGenericResult();
    if (val->IsLocalVar()) {
        /* `val` must be lambda, if we step in here, it means lambda's parent func is inlined
            e.g.
            func foo<T1>() {
                func goo<T2>() {}
                var a = goo<Int32>  // GetInstantiateValue(goo, T1, Int32)
            }
            main() {
                foo<Bool>()  // inlined
            }
            ===========> after inline, `main` body should be:
            main() {
                func goo<T2>() {}
                var a = goo<Int32>  // GetInstantiateValue(goo, Int32)
            }
            instantiated types in GetInstantiateValue are changed
        */
        auto oldOutDefTypes = GetOutDefDeclaredTypes(*result);
        auto oldAllTypes = GetInstantiateTypes();
        auto newAllTypes = GetOutDefDeclaredTypes(parent);
        for (size_t i = oldOutDefTypes.size(); i < oldAllTypes.size(); ++i) {
            newAllTypes.emplace_back(oldAllTypes[i]);
        }
        newNode = builder.CreateExpression<GetInstantiateValue>(result->GetType(), val, newAllTypes, &parent);
    } else {
        newNode = builder.CreateExpression<GetInstantiateValue>(result->GetType(), val, instantiateTys, &parent);
    }
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

std::vector<Type*> GetInstantiateValue::GetInstantiateTypes() const
{
    return instantiateTys;
}

Value* GetInstantiateValue::GetGenericResult() const
{
    return operands[0];
}

std::string GetInstantiateValue::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName();
    ss << "(";
    ss << GetGenericResult()->GetIdentifier();
    for (auto ty : GetInstantiateTypes()) {
        ss << ", " << ty->ToString();
    }
    ss << ")";
    return ss.str();
}

BlockGroup* Lambda::GetLambdaBody() const
{
    return body.GetBody();
}

void Lambda::AppendBlockGroup(BlockGroup& group)
{
    blockGroups.emplace_back(&group);
}

void Lambda::InitBody(BlockGroup& newBody)
{
    CJC_ASSERT(body.body == nullptr);
    CJC_ASSERT(blockGroups.empty());
    newBody.SetOwnerFunc(nullptr);
    newBody.SetOwnerExpression(this);
    body.body = &newBody;
}

FuncType* Lambda::GetFuncType() const
{
    return funcTy;
}

BlockGroup* Lambda::GetBody() const
{
    return body.GetBody();
}

void Lambda::RemoveBody()
{
    blockGroups.clear();
    body.RemoveBody();
}

Block* Lambda::GetEntryBlock() const
{
    return body.GetBody()->GetEntryBlock();
}

std::string Lambda::GetIdentifier() const
{
    return identifier;
}

/**
    * Get identifier without prefix '@'
    * '@' stands for global decl, so lambda's identifier doesn't have it
    */
std::string Lambda::GetIdentifierWithoutPrefix() const
{
    return identifier;
}

std::string Lambda::GetSrcCodeIdentifier() const
{
    return srcCodeIdentifier;
}

void Lambda::AddParam(Parameter& arg)
{
    body.AddParam(arg);
    arg.SetOwnerLambda(this);
}

size_t Lambda::GetNumOfParams() const
{
    return body.GetParams().size();
}

Parameter* Lambda::GetParam(size_t index) const
{
    return body.GetParam(index);
}

const std::vector<Parameter*>& Lambda::GetParams() const
{
    return body.GetParams();
}

const std::vector<GenericType*>& Lambda::GetGenericTypeParams() const
{
    return genericTypeParams;
}

void Lambda::SetReturnValue(LocalVar& ret)
{
    ret.SetRetValue();
    body.SetReturnValue(ret);
}

LocalVar* Lambda::GetReturnValue() const
{
    return body.GetReturnValue();
}

bool Lambda::IsLocalFunc() const
{
    return isLocalFunc;
}

void Lambda::SetParamDftValHostFunc(Lambda& hostFunc)
{
    paramDftValHostFunc = &hostFunc;
}

Lambda* Lambda::GetParamDftValHostFunc() const
{
    return paramDftValHostFunc;
}

void Lambda::SetCapturedVars(const std::vector<Value*>& vars)
{
    this->capturedVars = vars;
}

const std::vector<Value*>& Lambda::GetCapturedVars() const
{
    return capturedVars;
}

void Lambda::RemoveSelfFromBlock()
{
    if (auto blockGroup = body.GetBody()) {
        blockGroup->ClearBlockGroup();
        body.RemoveBody();
    }
    capturedVars.clear();

    Expression::RemoveSelfFromBlock();
}

std::string Debug::GetSrcCodeIdentifier() const
{
    return srcCodeIdentifier;
}

Value* Debug::GetValue() const
{
    return operands[0];
}

std::string GetRTTI::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName() << "(" << GetOperand()->GetIdentifier() << ")";
    return ss.str();
}

GetRTTI::GetRTTI(Value* val, Block* parent) : Expression{ExprKind::GET_RTTI, {val}, {}, parent}
{
    CJC_ASSERT(Is<RefType>(val->GetType()) &&
        (val->GetType()->StripAllRefs()->IsClass() || val->GetType()->StripAllRefs()->IsClass()) &&
        "GetRTTI must be used on class ref type");
}

GetRTTI* GetRTTI::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<GetRTTI>(GetDebugLocation(), GetResultType(), GetOperand(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

std::string GetRTTIStatic::ToString(unsigned indent) const
{
    (void)indent;
    std::stringstream ss;
    ss << GetExprKindName() << "(" << ty->ToString() << ")";
    return ss.str();
}

GetRTTIStatic* GetRTTIStatic::Clone(CHIRBuilder& builder, Block& parent) const
{
    auto newNode = builder.CreateExpression<GetRTTIStatic>(GetDebugLocation(), GetResultType(), GetRTTIType(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

void GetRTTIStatic::ReplaceRTTIType(Type* type)
{
    ty = type;
}
