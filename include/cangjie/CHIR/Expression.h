// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the Expression class in CHIR.
 */

#ifndef CANGJIE_CHIR_EXPRESSION_H
#define CANGJIE_CHIR_EXPRESSION_H

#include "cangjie/CHIR/IntrinsicKind.h"
#include "cangjie/CHIR/LiteralValue.h"
#include "cangjie/Utils/SafePointer.h"

#include <cstdarg>
#include <list>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace Cangjie::CHIR {
class CHIRBuilder;
class ExprTypeConverter;

/**
 * Expression major kind.
 */
enum class ExprMajorKind : uint8_t {
#define EXPRKIND(...)
#define MAJORKIND(KIND, ...) KIND,
#include "cangjie/CHIR/ExprKind.inc"
#undef EXPRKIND
#undef MAJORKIND
};

/**
 * Expression minor kind.
 */
enum class ExprKind : uint8_t {
    INVALID = 0,
#define EXPRKIND(KIND, ...) KIND,
#define MAJORKIND(_1, ...) __VA_ARGS__
#include "cangjie/CHIR/ExprKind.inc"
#undef EXPRKIND
#undef MAJORKIND
    MAX_EXPR_KINDS
};

enum class SourceExpr {
    IF_EXPR,
    WHILE_EXPR,
    DO_WHILE_EXPR,
    MATCH_EXPR,
    IF_LET_OR_WHILE_LET,
    QUEST,
    BINARY,
    FOR_IN_EXPR,
    OTHER
};

class ExprKindMgr {
public:
    static const ExprKindMgr* Instance();
    /**
     * @brief Get major kind which the `exprKind` belongs to
     *
     * @param exprKind defined in file "cangjie/CHIR/ExprKind.inc"
     *
     */
    ExprMajorKind GetMajorKind(ExprKind exprKind) const;

    /**
     * @brief Get kind name
     *
     * @param exprKind defined in file "cangjie/CHIR/ExprKind.inc"
     *
     */
    std::string GetKindName(size_t exprKind) const;

private:
    void InitMap(ExprMajorKind majorKind, ...);
    ExprKindMgr();
    /**
     * Generate names of Expression minor kind.
     */
    const char* exprKindNames[static_cast<size_t>(Cangjie::CHIR::ExprKind::MAX_EXPR_KINDS)] = {
        "INVALID",
#define EXPRKIND(KIND, NAME, ...) NAME,
#define MAJORKIND(_1, ...) __VA_ARGS__
#include "cangjie/CHIR/ExprKind.inc"
#undef EXPRKIND
#undef MAJORKIND
    };
    std::unordered_map<ExprKind, ExprMajorKind> expr2MajorExprMap;
};

constexpr size_t WITH_EXCEPTION_SUCCESSOR_NUM = 2;
/*
 * Expression is a variety of "Expression"'s base class in CHIR, like BinaryExpression、Apply.
 * the subclasses of this class mainly represent various "operations" on Value.
 */
class Expression : public Base {
    friend class CHIRContext;
    friend class CHIRBuilder;
    friend class Value;
    friend class LocalVar;
    friend class Block;
    friend class BlockGroup;
    friend class ExprTypeConverter;

public:
    /**
     * @brief Get major kind which the `kind` belongs to
     */
    ExprMajorKind GetExprMajorKind() const;

    ExprKind GetExprKind() const;

    bool IsConstant() const;
    bool IsConstantNull() const;
    bool IsConstantBool() const;
    bool IsConstantInt() const;
    bool IsConstantFloat() const;
    bool IsConstantString() const;

    bool IsTerminator() const;

    bool IsUnaryExpr() const;

    bool IsBinaryExpr() const;

    bool IsMemoryExpr() const;

    bool IsStructuredControlFlowExpr() const;

    bool IsOtherExpr() const;

    /**
     * @brief Get expression kind name
     */
    std::string GetExprKindName() const;

    /**
     * @brief Retrieves the major and minor expression kinds.
     *
     * @return A pair containing the major and minor expression kinds.
     */
    std::pair<ExprMajorKind, ExprKind> GetMajorAndMinorExprKind() const;

    /**
     * @brief Retrieves the parent block of the expression.
     *
     * @return The parent block of the expression.
     */
    Block* GetParent() const;

    /**
     * @brief Retrieves the parent function of the expression.
     *
     * @return The parent function of the expression.
     */
    Func* GetParentFunc() const;

    /**
     * @brief Retrieves a block group by index.
     *
     * @param idx The index of the block group to retrieve.
     * @return The block group at the specified index.
     */
    BlockGroup* GetBlockGroup(unsigned idx) const;

    /**
     * @brief Retrieves the number of block groups.
     *
     * @return The number of block groups.
     */
    size_t GetNumOfBlockGroups() const;

    /**
     * @brief Retrieves all block groups.
     *
     * @return A vector of pointers to all block groups.
     */
    const std::vector<BlockGroup*>& GetBlockGroups() const;

    /**
     * @brief Retrieves the number of operands.
     *
     * @return The number of operands.
     */
    virtual size_t GetNumOfOperands() const;

    /**
     * @brief Retrieves all operands.
     *
     * @return A vector of pointers to all operands.
     */
    virtual std::vector<Value*> GetOperands() const;

    /**
     * @brief Retrieves an operand by index.
     *
     * @param idx The index of the operand to retrieve.
     * @return The operand at the specified index.
     */
    virtual Value* GetOperand(size_t idx) const;

    /**
     * @brief Retrieves the result of the expression.
     *
     * @return The result of the expression.
     */
    LocalVar* GetResult() const;

    /**
     * @brief Retrieves the result type of the expression.
     *
     * @return The result type of the expression.
     */
    Type* GetResultType() const;
    
    /**
     * @brief Retrieves the parent block group of the expression.
     *
     * @return The parent block group of the expression.
     */
    BlockGroup* GetParentBlockGroup() const;

    /**
     * @brief Break all connection with its parent and operands
     *
     * that means you can not get its parent and operands any more, and you can not get this expression by its parent
     * and its operands, too. But we don't free this expression's memory.
     */
    virtual void RemoveSelfFromBlock();

    /**
     * @brief Remove this expression from its parent basicblock, and insert it into before the @expr expression.
     *
     * @param expr The destination position which is before this reference expression.
     *
     */
    void MoveBefore(Expression* expr);

    /**
     * @brief Remove this expression from its parent basicblock, and insert it into after the @expr expression.
     *
     * @param expr The destination position which is after this reference expression.
     *
     */
    void MoveAfter(Expression* expr);

    /**
     * @brief Move this expression from current parent block to another one.
     * Break connection with current parent block and rebuild connection with the new one, the connection with
     * its operands is not changed
     *
     * @param block the new block which the expression belongs to
     */
    void MoveTo(Block& block);

    /**
     * @brief Replace any uses of @from with @to in this expression.
     *
     * @param from The source value.
     * @param to The destination value.
     *
     */
    void ReplaceOperand(Value* oldOperand, Value* newOperand);
    void ReplaceOperand(size_t idx, Value* newOperand);

    /**
     * @brief Translate into string for expression.
     *
     * @param debug Whether to include debugging info.
     * @param detail Whether to include details.
     * @return The string of the expression.
     */
    virtual std::string ToString(unsigned indent = 0) const;

    bool IsCompileTimeValue() const;

    void SetCompileTimeValue();

    virtual void ReplaceWith(Expression& newExpr);
    void InsertBefore(Expression& baseExpr);

    void Dump() const;
    virtual Expression* Clone(CHIRBuilder& builder, Block& parent) const = 0;

protected:
    explicit Expression(
        ExprKind kind, const std::vector<Value*>& operands, const std::vector<BlockGroup*>& blockGroups, Block* parent);
    explicit Expression(ExprKind kind, const std::vector<Value*>& operands, Block* parent)
        : Expression(kind, operands, {}, parent)
    {
    }
    explicit Expression(const Expression& other, Block* parent)
        : Expression(other.kind, other.operands, other.blockGroups, parent)
    {
    }
    virtual ~Expression() = default;

    Expression& operator=(const Expression&) = delete;

    void AppendOperand(Value& op);

    void SetParent(Block* newParent);

    /**
     * @brief Remove use relataions between this expression and it's operands.
     */
    void EraseOperands();

    ExprKind kind;                        // Expression kind.
    std::vector<Value*> operands;         // The operands.
    std::vector<BlockGroup*> blockGroups; // The regions of special expression, such as Func.
    Block* parent;                        // The owner basicblock of this expression.
    LocalVar* result = nullptr;           // The result.
    bool isCompileTimeValue = false;
};

/*
 * @brief UnaryExpression class in CHIR.
 *
 * ExprKind can only be ExprKind::NEG and ExprKind::NOT
 */
class UnaryExpression : public Expression {
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    using Expression::GetOperand;
    Value* GetOperand() const;

    OverflowStrategy GetOverflowStrategy() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~UnaryExpression() override = default;

private:
    explicit UnaryExpression(ExprKind kind, Value* operand, Cangjie::OverflowStrategy ofs, Block* parent)
        : Expression(kind, {operand}, {}, parent), overflowStrategy(ofs)
    {
    }
    explicit UnaryExpression(ExprKind kind, Value* operand, Block* parent) : Expression(kind, {operand}, {}, parent)
    {
    }

    UnaryExpression* Clone(CHIRBuilder& builder, Block& parent) const override;

    Cangjie::OverflowStrategy overflowStrategy{Cangjie::OverflowStrategy::NA};
};

/*
 * @brief BinaryExpression class in CHIR.
 *
 * ExprKind should be between ExprKind::ADD and ExprKind::OR
 */
class BinaryExpression : public Expression {
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    /**
     * @brief Retrieves the left-hand side operand.
     *
     * @return The left-hand side operand.
     */
    Value* GetLHSOperand() const;

    /**
     * @brief Retrieves the right-hand side operand.
     *
     * @return The right-hand side operand.
     */
    Value* GetRHSOperand() const;

    OverflowStrategy GetOverflowStrategy() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~BinaryExpression() override = default;

private:
    explicit BinaryExpression(ExprKind kind, Value* lhs, Value* rhs, Cangjie::OverflowStrategy ofs, Block* parent)
        : Expression(kind, {lhs, rhs}, {}, parent), overflowStrategy(ofs)
    {
    }
    explicit BinaryExpression(ExprKind kind, Value* lhs, Value* rhs, Block* parent)
        : Expression(kind, {lhs, rhs}, {}, parent)
    {
    }

    BinaryExpression* Clone(CHIRBuilder& builder, Block& parent) const override;

    Cangjie::OverflowStrategy overflowStrategy{Cangjie::OverflowStrategy::NA};
};

/*
 * @brief Constant class in CHIR.
 *
 * Create a constant from a literal or func
 *
 */
class Constant : public Expression {
public:
    LiteralValue* GetValue() const;

    bool IsFuncLit() const;

    const std::vector<GenericType*>& GetGenericArgs() const;

    std::string ToString(unsigned indent = 0) const override;

    // ======================== constant bool ======================== //

    bool GetBoolLitVal() const;

    bool IsBoolLit() const;

    // ======================== constant char ======================== //

    char32_t GetRuneLitVal() const;

    bool IsRuneLit() const;

    // ======================== constant string ======================== //

    std::string GetStringLitVal() const;

    bool IsStringLit() const;

    // ======================== constant int ======================== //

    int64_t GetSignedIntLitVal() const;

    uint64_t GetUnsignedIntLitVal() const;

    bool IsIntLit() const;

    bool IsSignedIntLit() const;

    // ======================== constant float ======================== //

    bool IsFloatLit() const;

    double GetFloatLitVal() const;

    bool IsNullLit() const;

    bool IsUnitLit() const;

protected:
    ~Constant() override = default;

private:
    explicit Constant(LiteralValue* val, Block* parent) : Expression(ExprKind::CONSTANT, {val}, {}, parent)
    {
    }
    explicit Constant(Func* fval, Block* parent, const std::vector<GenericType*>& genericArgs = {})
        : Expression(ExprKind::CONSTANT, {fval}, {}, parent), genericArgs(genericArgs)
    {
    }

    Constant* Clone(CHIRBuilder& builder, Block& parent) const override;
    friend class CHIRContext;
    friend class CHIRBuilder;
    std::vector<GenericType*> genericArgs; // generic arg types
};

/*
 * @brief Allocate class in CHIR.
 *
 * kind is ExprKind::ALLOCATE
 */
class Allocate : public Expression {
    friend class ExprTypeConverter;
public:
    /**
     * @brief which type this `Allocate` allocates
     */
    Type* GetType() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~Allocate() override = default;

private:
    explicit Allocate(Type* ty, Block* parent) : Expression(ExprKind::ALLOCATE, {}, {}, parent), ty(ty)
    {
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    Allocate* Clone(CHIRBuilder& builder, Block& parent) const override;

    Type* ty; // The type to be allocated.
};

/**
 * Load a value from reference
 */
class Load : public Expression {
public:
    /**
     * @brief which reference to be load
     */
    Value* GetLocation() const;

protected:
    ~Load() override = default;

private:
    explicit Load(Value* location, Block* parent) : Expression(ExprKind::LOAD, {location}, {}, parent)
    {
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    Load* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief store a value to a reference.
 */
class Store : public Expression {
public:
    Value* GetValue() const;
    Value* GetLocation() const;

protected:
    ~Store() override = default;

private:
    explicit Store(Value* val, Value* location, Block* parent)
        : Expression(ExprKind::STORE, {val, location}, {}, parent)
    {
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    Store* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * Compute the reference to the corresponding child element data
 * from the data specified by `Location` base on `Path`.
 */
class GetElementRef : public Expression {
public:
    Value* GetLocation() const;

    const std::vector<uint64_t>& GetPath() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~GetElementRef() override = default;

private:
    explicit GetElementRef(Value* location, const std::vector<uint64_t>& path, Block* parent)
        : Expression(ExprKind::GET_ELEMENT_REF, {location}, {}, parent), indexes(path)
    {
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    GetElementRef* Clone(CHIRBuilder& builder, Block& parent) const override;
    /*
     * @brief The path of location.
     *
     */
    std::vector<uint64_t> indexes;
};

/*
 * @brief StoreElementRef class in CHIR.
 *
 * kind is ExprKind::STORE_ELEMENT_REF
 */
class StoreElementRef : public Expression {
public:
    /**
     * @brief Retrieves the value.
     *
     * @return The value.
     */
    Value* GetValue() const;
    
    /**
     * @brief Retrieves the location.
     *
     * @return The location.
     */
    Value* GetLocation() const;

    /**
     * @brief Retrieves the path.
     *
     * @return A vector containing the path.
     */
    const std::vector<uint64_t>& GetPath() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~StoreElementRef() override = default;

private:
    explicit StoreElementRef(Value* value, Value* location, const std::vector<uint64_t>& path, Block* parent)
        : Expression(ExprKind::STORE_ELEMENT_REF, {value, location}, {}, parent), indexes(path)
    {
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    StoreElementRef* Clone(CHIRBuilder& builder, Block& parent) const override;
    /*
     * @brief The path of location.
     *
     */
    std::vector<uint64_t> indexes;
};

struct CalleeInfo {
    // instantiated parent custom type of callee func if exists
    Type* instParentCustomTy{nullptr};
    Type* thisType{nullptr};
    // instantiated param type of callee func
    std::vector<Type*> instParamTys;
    // instantiated return type of callee func
    Type* instRetTy{nullptr};
};

class Apply : public Expression {
    friend class ExprTypeConverter;
public:
    /**
     * @brief Retrieves the callee of the application.
     *
     * @return The callee of the application.
     */
    Value* GetCallee() const;

    /**
     * @brief Retrieves a list of the application argument nodes.
     *
     * @return A vector of pointers to the application argument nodes.
     */
    std::vector<Value*> GetArgs() const;

    std::string ToString(unsigned indent = 0) const override;

    /**
     * @brief Sets whether the call is a super call.
     *
     * @param newIsSuperCall True if the call is a super call, false otherwise.
     */
    void SetSuperCall(bool newIsSuperCall);

    /**
     * @brief Checks if the call is a super call.
     *
     * @return True if the call is a super call, false otherwise.
     */
    bool IsSuperCall() const;

    /**
     * @brief Sets the instantiated argument types.
     *
     * @param args A vector of pointers to the argument types.
     */
    void SetInstantiatedArgTypes(const std::vector<Type*>& args);

    /**
     * @brief Sets the instantiated argument types (deprecated).
     *
     * @param args A vector of pointers to the argument types.
     */
    void SetInstantiatedArgTypes(const std::vector<Ptr<Type>>& args);

    /**
     * @brief Sets the instantiated function type.
     *
     * @param thisTy The type of 'this'.
     * @param instParentCustomTy The instantiated parent custom type.
     * @param paramTys A vector of parameter types.
     * @param retTy The return type.
     */
    void SetInstantiatedFuncType(
        Type* thisTy, Type* instParentCustomTy, std::vector<Type*> paramTys, Type& retTy);

    /**
     * @brief Sets the instantiated return type.
     *
     * @param retTy The return type.
     */
    void SetInstantiatedRetType(Type& retTy);

    /**
     * @brief Sets the instantiated parameter type.
     *
     * @param idx The index of the parameter.
     * @param paramTy The parameter type.
     */
    void SetInstantiatedParamType(size_t idx, Type& paramTy);

    /**
     * @brief Retrieves the type of 'this'.
     *
     * @return The type of 'this'.
     */
    Type* GetThisType() const;

    /**
     * @brief Sets the type of 'this'.
     *
     * @param thisType The type of 'this'.
     */
    void SetThisType(Type& thisType);

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    Type* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Sets the instantiated parent custom type of the callee.
     *
     * @param parentType The parent custom type.
     */
    void SetInstParentCustomTyOfCallee(Type& parentType);

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Retrieves the instantiated argument types.
     *
     * @return A vector of pointers to the instantiated argument types.
     */
    const std::vector<Type*>& GetInstantiateArgs() const;

    /**
     * @brief Retrieves the callee type information.
     *
     * @return The callee type information.
     */
    CalleeInfo GetCalleeTypeInfo() const;

protected:
    ~Apply() override = default;

private:
    explicit Apply(Value* callee, const std::vector<Value*>& args, Block* parent);
    friend class CHIRContext;
    friend class CHIRBuilder;

    Apply* Clone(CHIRBuilder& builder, Block& parent) const override;

    /**
     * instantiate types args of generic types, non-empty if user uses --disable-instantiate
     */
    std::vector<Type*> instantiateArgs; // instantiate types args of generic types.
    bool isSuperCall{false};
    CalleeInfo instFuncType;
};

struct InvokeCalleeInfo {
    std::string srcCodeIdentifier;
    FuncType* instFuncType{nullptr};     // not ()->Unit, include this type
    FuncType* originalFuncType{nullptr}; // not ()->Unit, include this type
    ClassType* instParentCustomTy{nullptr};
    ClassType* originalParentCustomTy{nullptr};
    std::vector<Type*> instantiatedTypeArgs;
    Type* thisType{nullptr};
    size_t offset{0};
};

class Invoke : public Expression {
    friend class ExprTypeConverter;
    friend class PrivateTypeConverterNoInvokeOriginal;
public:
    /**
     * @brief Retrieves the object of this Invoke operation.
     *
     * @return The object of this Invoke operation.
     */
    Value* GetObject() const;

    /**
     * @brief Retrieves the method name of this Invoke operation.
     *
     * @return The method name of this Invoke operation.
     */
    std::string GetMethodName() const;

    /**
     * @brief Retrieves the method type of this Invoke operation.
     *
     * @return The method type of this Invoke operation.
     */
    FuncType* GetMethodType() const;

    /**
     * @brief Retrieves the call arguments of this Invoke operation.
     *
     * @return A vector of pointers to the call arguments.
     */
    std::vector<Value*> GetArgs() const;

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    ClassType* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Sets the instantiated function type.
     *
     * @param funcType The function type.
     */
    void SetInstantiatedFuncType(FuncType& funcType);

    /**
     * @brief Retrieves the instantiated type arguments.
     *
     * @return A vector of pointers to the instantiated type arguments.
     */
    std::vector<Type*> GetInstantiatedTypeArgs() const;

    /**
     * @brief Sets the function information.
     *
     * @param info The function information.
     */
    void SetFuncInfo(const InvokeCalleeInfo& info);

    /**
     * @brief Retrieves the function information.
     *
     * @return The function information.
     */
    InvokeCalleeInfo GetFuncInfo() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~Invoke() override = default;

private:
    explicit Invoke(Value* object, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo, Block* parent);

    friend class CHIRContext;
    friend class CHIRBuilder;

    Invoke* Clone(CHIRBuilder& builder, Block& parent) const override;

    InvokeCalleeInfo funcInfo;
};

/// Get RTTI of its operand.
struct GetRTTI : public Expression {
    Value* GetOperand() const { return operands[0]; }
    using Expression::GetOperand;

    std::string ToString(unsigned indent = 0) const override;

    GetRTTI(Value* val, Block* parent);
    ~GetRTTI() override = default;
    GetRTTI* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/// Get RTTI value of This type. This expression can only be used in static member function.
struct GetRTTIStatic : public Expression {
    std::string ToString(unsigned indent = 0) const override;

    Type* GetRTTIType() const { return ty; }

    GetRTTIStatic(Type* type, Block* parent) : Expression{ExprKind::GET_RTTI_STATIC, {}, {}, parent}, ty{type} {}
    ~GetRTTIStatic() override = default;
    GetRTTIStatic* Clone(CHIRBuilder& builder, Block& parent) const override;

    void ReplaceRTTIType(Type* type);
private:
    Type* ty;
};

class InvokeStatic : public Expression {
    friend class ExprTypeConverter;
    friend class PrivateTypeConverterNoInvokeOriginal;
public:
    /**
     * @brief Retrieves the method name of this Invoke operation.
     *
     * @return The method name of this Invoke operation.
     */
    std::string GetMethodName() const;

    /**
     * @brief Retrieves the method type of this Invoke operation.
     *
     * @return The method type of this Invoke operation.
     */
    FuncType* GetMethodType() const;

    /**
     * @brief Retrieves the call arguments of this Invoke operation.
     *
     * @return A vector of pointers to the call arguments.
     */
    std::vector<Value*> GetArgs() const;

    /**
     * @brief Retrieves the RTTI value.
     *
     * @return The RTTI value.
     */
    Value* GetRTTIValue() const;

    /**
     * @brief Retrieves the type of 'this'.
     *
     * @return The type of 'this'.
     */
    Type* GetThisType() const;

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    Type* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Sets the instantiated function type.
     *
     * @param funcType The function type.
     */
    void SetInstantiatedFuncType(FuncType& funcType);

    /**
     * @brief Retrieves the instantiated type arguments.
     *
     * @return A vector of pointers to the instantiated type arguments.
     */
    std::vector<Type*> GetInstantiatedTypeArgs() const;

    /**
     * @brief Sets the function information.
     *
     * @param info The function information.
     */
    void SetFuncInfo(const InvokeCalleeInfo& info);

    /**
     * @brief Retrieves the function information.
     *
     * @return The function information.
     */
    InvokeCalleeInfo GetFuncInfo() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~InvokeStatic() override = default;

private:
    explicit InvokeStatic(
        Value* rtti, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo, Block* parent);

    friend class CHIRContext;
    friend class CHIRBuilder;

    InvokeStatic* Clone(CHIRBuilder& builder, Block& parent) const override;

    InvokeCalleeInfo funcInfo;
};

class TypeCast : public Expression {
public:
    /** @brief Get the source value of this cast operation */
    Value* GetSourceValue() const;

    /** @brief Get the source type of this cast operation */
    Type* GetSourceTy() const;

    /** @brief Get the target type of this cast operation */
    Type* GetTargetTy() const;

    /** @brief Get the overflow strategy of this cast operation */
    OverflowStrategy GetOverflowStrategy() const;

    std::string ToString(unsigned indent = 0) const override;

protected:
    ~TypeCast() override = default;

private:
    explicit TypeCast(Value* operand, Block* parent) : Expression(ExprKind::TYPECAST, {operand}, {}, parent)
    {
    }
    explicit TypeCast(Value* operand, Cangjie::OverflowStrategy overflow, Block* parent)
        : Expression(ExprKind::TYPECAST, {operand}, {}, parent), overflowStrategy(overflow)
    {
    }

    friend class CHIRContext;
    friend class CHIRBuilder;

    TypeCast* Clone(CHIRBuilder& builder, Block& parent) const override;

    Cangjie::OverflowStrategy overflowStrategy{Cangjie::OverflowStrategy::NA};
};

class InstanceOf : public Expression {
    friend class ExprTypeConverter;
protected:
    ~InstanceOf() override = default;

private:
    explicit InstanceOf(Value* operand, Type* ty, Block* parent)
        : Expression(ExprKind::INSTANCEOF, {operand}, {}, parent), ty(ty)
    {
    }

    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    /** @brief Get the object of this Instanceof operation */
    Value* GetObject() const;

    /** @brief Get the class type of this Instanceof operation */
    Type* GetType() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    InstanceOf* Clone(CHIRBuilder& builder, Block& parent) const override;
    Type* ty;
};

class Box : public Expression {
private:
    explicit Box(Value* operand, Block* parent)
        : Expression(ExprKind::BOX, {operand}, {}, parent)
    {
    }
    ~Box() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    /** @brief Get the object of this Box operation */
    Value* GetObject() const;

    Type* GetSourceTy() const;

    Type* GetTargetTy() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    Box* Clone(CHIRBuilder& builder, Block& parent) const override;
};

class UnBox : public Expression {
private:
    explicit UnBox(Value* operand, Block* parent)
        : Expression(ExprKind::UNBOX, {operand}, {}, parent)
    {
    }
    ~UnBox() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    /** @brief Get the object of this Box operation */
    Value* GetObject() const;

    Type* GetSourceTy() const;

    Type* GetTargetTy() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    UnBox* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Transform from concrete type to generic type
 *     1. Transforming from Int64 to T need TransformToGeneric if T is a geneirc type
 *     2. TransformToGeneric expression is no need from C<Int> to C<T> when C is a classlike structure.
 */
class TransformToGeneric : public Expression {
private:
    explicit TransformToGeneric(Value* operand, Block* parent)
        : Expression(ExprKind::TRANSFORM_TO_GENERIC, {operand}, {}, parent)
    {
    }
    ~TransformToGeneric() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    Value* GetObject() const;

    Type* GetSourceTy() const;

    Type* GetTargetTy() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    TransformToGeneric* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Transform from generic type to concrete type
 *     1. Transforming from T to Int64 need TransformToConcrete if T is a geneirc type
 *     2. TransformToConcrete expression is no need from C<T> to C<Int64> when C is a classlike structure.
 */
class TransformToConcrete : public Expression {
private:
    explicit TransformToConcrete(Value* operand, Block* parent)
        : Expression(ExprKind::TRANSFORM_TO_CONCRETE, {operand}, {}, parent)
    {
    }
    ~TransformToConcrete() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    Value* GetObject() const;

    Type* GetSourceTy() const;

    Type* GetTargetTy() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    TransformToConcrete* Clone(CHIRBuilder& builder, Block& parent) const override;
};

class UnBoxToRef : public Expression {
private:
    explicit UnBoxToRef(Value* operand, Block* parent)
        : Expression(ExprKind::UNBOX_TO_REF, {operand}, {}, parent)
    {
    }
    ~UnBoxToRef() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

public:
    Value* GetObject() const;

    Type* GetSourceTy() const;

    Type* GetTargetTy() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    UnBoxToRef* Clone(CHIRBuilder& builder, Block& parent) const override;
};


/*
 * @brief Terminator class in CHIR.
 *
 */
class Terminator : public Expression {
    friend class Block;

public:
    size_t GetNumOfSuccessor() const;

    Block* GetSuccessor(size_t index) const;

    size_t GetNumOfOperands() const override;

    std::vector<Value*> GetOperands() const override;

    Value* GetOperand(size_t idx) const override;

    void ReplaceWith(Expression& newTerminator) override;
    /**
     * @brief Replace `oldSuccessor` with `newSuccessor`
     *
     * @param oldSuccessor: one of the current successors
     * @param newSuccessor: new successor
     */
    void ReplaceSuccessor(Block& oldSuccessor, Block& newSuccessor);

    /**
     * @brief Replaced the successor of this terminator
     *
     * @param index: the index-th successor need to be replaced
     * @param newSuccessor: new successor
     */
    void ReplaceSuccessor(size_t index, Block& newSuccessor);

    const std::vector<Block*> GetSuccessors() const;

    /**
     * @brief Break all connection with its parent and operands
     * that means you can not get its parent and operands any more, and you can not get this terminator by its parent
     * and its operands, too. But we don't free this terminator's memory.
     */
    void RemoveSelfFromBlock() override;

    std::string ToString(unsigned indent = 0) const override;
    Terminator* Clone(CHIRBuilder& builder, Block& parent) const override;
protected:
    explicit Terminator(
        ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent);
    ~Terminator() override = default;

    void AppendSuccessor(Block& block);

    std::string SuffixString() const;

    /**
     * @brief Get first successor's index in operands.
     *
     * in another sense, return the num of operand(does not include successors).
     *
     * NOTE: because returned value is constant,
     * call this function only after Terminator's full initialization(operand and successor are properly set).
     */
    size_t GetFirstSuccessorIndex() const;

private:
    void LetSuccessorsRemoveCurBlock() const;
};

/*
 * @brief GoTo terminator class in CHIR.
 *
 * Unconditional Branch Expression with a successor.
 *
 * kind is ExprKind::GOTO
 *
 */
class GoTo : public Terminator {
public:
    Block* GetDestination() const;

private:
    explicit GoTo(Block* destBlock, Block* parent) : Terminator(ExprKind::GOTO, {}, {destBlock}, parent)
    {
    }
    ~GoTo() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    GoTo* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Branch terminator class in CHIR.
 *
 * Conditional Branch Expression with 2 successors.
 *
 * kind is ExprKind::BRANCH
 *
 */
class Branch : public Terminator {
public:
    /** @brief Get the condition of this Branch Expression */
    Value* GetCondition() const;

    /** @brief Get the true block of this Branch Expression */
    Block* GetTrueBlock() const;

    /** @brief Get the false block of this Branch Expression */
    Block* GetFalseBlock() const;

    SourceExpr sourceExpr = SourceExpr::OTHER;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Branch(Value* cond, Block* trueBlock, Block* falseBlock, Block* parent)
        : Terminator(ExprKind::BRANCH, {cond}, {trueBlock, falseBlock}, parent)
    {
    }
    ~Branch() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Branch* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief MultiBranch terminator class in CHIR.
 *
 * MultiBranch Expression with at least one successor.
 *
 * kind is ExprKind::MultiBranch
 *
 */
class MultiBranch : public Terminator {
public:
    /** @brief Get the condition of this MultiBranch Expression */
    Value* GetCondition() const;

    /** @brief Get the case values of this MultiBranch Expression */
    const std::vector<uint64_t>& GetCaseVals() const;

    /** @brief Get the case value by index of this MultiBranch Expression */
    uint64_t GetCaseValByIndex(size_t index) const;

    /** @brief Get the case block by index of this MultiBranch Expression */
    Block* GetCaseBlockByIndex(size_t index) const;

    /** @brief Get the default block of this MultiBranch Expression */
    Block* GetDefaultBlock() const;

    std::vector<Block*> GetNormalBlocks() const;

private:
    explicit MultiBranch(Value* cond, Block* defaultBlock, const std::vector<uint64_t>& vals,
        const std::vector<Block*>& succs, Block* parent);
    ~MultiBranch() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    std::string ToString(unsigned indent = 0) const override;

    MultiBranch* Clone(CHIRBuilder& builder, Block& parent) const override;

    /**
     * @brief The specific case values used to match.
     * Note that default Block does not have the case val.
     */
    std::vector<uint64_t> caseVals;
};

/*
 * @brief Exit terminator class in CHIR.
 *
 * Exit Expression with a successor.
 *
 * kind is ExprKind::EXIT
 *
 */
class Exit : public Terminator {
private:
    explicit Exit(Block* parent) : Terminator(ExprKind::EXIT, {}, {}, parent)
    {
    }

    ~Exit() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    Exit* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief RaiseException terminator class in CHIR.
 *
 * RaiseException Expression with zero or one successor.
 *
 * kind is ExprKind::RAISE_EXCEPTION
 *
 */
class RaiseException : public Terminator {
public:
    /** @brief Get the exception value of this RaiseException Expression */
    Value* GetExceptionValue() const;

    /**
     * @brief Get the exception block of this RaiseException Expression.
     *
     *  Return exception block if exist,
     *  nullptr,  otherwise.
     */
    Block* GetExceptionBlock() const;

private:
    explicit RaiseException(Value* value, Block* parent) : Terminator(ExprKind::RAISE_EXCEPTION, {value}, {}, parent)
    {
    }
    explicit RaiseException(Value* value, Block* successor, Block* parent)
        : Terminator(ExprKind::RAISE_EXCEPTION, {value}, {successor}, parent)
    {
    }
    ~RaiseException() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    RaiseException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief ApplyWithException terminator class in CHIR.
 *
 * ApplyWithException Expression with one or two successors.
 *
 * kind is ExprKind::APPLY_WITH_EXCEPTION
 *
 */
class ApplyWithException : public Terminator {
    friend class ExprTypeConverter;
public:
    /**
     * @brief Retrieves the callee of this ApplyWithException operation.
     *
     * @return The callee of this ApplyWithException operation.
     */
    Value* GetCallee() const;

    /**
     * @brief Retrieves a list of the ApplyWithException operation argument nodes.
     *
     * @return A vector of pointers to the argument nodes.
     */
    std::vector<Value*> GetArgs() const;

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

    /**
     * @brief Sets the instantiated argument types.
     *
     * @param args A vector of pointers to the argument types.
     */
    void SetInstantiatedArgTypes(const std::vector<Type*>& args);

    /**
     * @brief Sets the instantiated argument types (deprecated).
     *
     * @param args A vector of pointers to the argument types.
     */
    void SetInstantiatedArgTypes(const std::vector<Ptr<Type>>& args);

    /**
     * @brief Sets the instantiated function type.
     *
     * @param thisTy The type of 'this' (nullptr if callee is not a member function).
     * @param instParentCustomTy The instantiated parent custom type.
     * @param paramTys A vector of parameter types.
     * @param retTy The return type.
     */
    void SetInstantiatedFuncType(
        Type* thisTy, Type* instParentCustomTy, const std::vector<Type*>& paramTys, Type& retTy);

    /**
     * @brief Sets the instantiated return type.
     *
     * @param retTy The return type.
     */
    void SetInstantiatedRetType(Type& retTy);

    /**
     * @brief Sets the instantiated parameter type.
     *
     * @param idx The index of the parameter.
     * @param paramTy The parameter type.
     */
    void SetInstantiatedParamType(size_t idx, Type& paramTy);

    /**
     * @brief Retrieves the type of 'this'.
     *
     * @return The type of 'this'.
     */
    Type* GetThisType() const;

    /**
     * @brief Sets the type of 'this'.
     *
     * @param thisType The type of 'this'.
     */
    void SetThisType(Type& thisType);

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    Type* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Sets the instantiated parent custom type of the callee.
     *
     * @param parentType The parent custom type.
     */
    void SetInstParentCustomTyOfCallee(Type& parentType);

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Retrieves the instantiated argument types.
     *
     * @return A vector of pointers to the instantiated argument types.
     */
    const std::vector<Type*>& GetInstantiateArgs() const;

    /**
     * @brief Retrieves the callee type information.
     *
     * @return The callee type information.
     */
    CalleeInfo GetCalleeTypeInfo() const;

private:
    explicit ApplyWithException(
        Value* callee, const std::vector<Value*>& args, Block* sucBlock, Block* errBlock, Block* parent);
    ~ApplyWithException() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    std::string ToString(unsigned indent = 0) const override;

    ApplyWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

private:
    std::vector<Type*> instantiateArgs; // generic arg types
    CalleeInfo instFuncType;
};

/*
 * @brief InvokeWithException terminator class in CHIR.
 *
 * InvokeWithException Expression with one or two successors.
 *
 * kind is ExprKind::INVOKE_WITH_EXCEPTION
 *
 */
class InvokeWithException : public Terminator {
    friend class ExprTypeConverter;
    friend class PrivateTypeConverterNoInvokeOriginal;
public:
    /**
     * @brief Retrieves the object of this InvokeWithException operation.
     *
     * @return The object of this InvokeWithException operation.
     */
    Value* GetObject() const;

    /**
     * @brief Retrieves the method name of this Invoke operation.
     *
     * @return The method name of this Invoke operation.
     */
    std::string GetMethodName() const;

    /**
     * @brief Retrieves the method type of this Invoke operation.
     *
     * @return The method type of this Invoke operation.
     */
    FuncType* GetMethodType() const;

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    Type* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Sets the instantiated function type.
     *
     * @param funcType The function type.
     */
    void SetInstantiatedFuncType(FuncType& funcType);

    /**
     * @brief Retrieves the instantiated type arguments.
     *
     * @return A vector of pointers to the instantiated type arguments.
     */
    std::vector<Type*> GetInstantiatedTypeArgs() const;

    /**
     * @brief Sets the function information.
     *
     * @param info The function information.
     */
    void SetFuncInfo(const InvokeCalleeInfo& info);

    /**
     * @brief Retrieves the function information.
     *
     * @return The function information.
     */
    InvokeCalleeInfo GetFuncInfo() const;

    /**
     * @brief Retrieves the call arguments of this InvokeWithException operation.
     *
     * @return A vector of pointers to the call arguments.
     */
    std::vector<Value*> GetArgs() const;

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit InvokeWithException(Value* object, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo,
        Block* sucBlock, Block* errBlock, Block* parent);

    ~InvokeWithException() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    std::string ToString(unsigned indent = 0) const override;

    InvokeWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    InvokeCalleeInfo funcInfo;
};

/*
 * @brief InvokeStaticWithException terminator class in CHIR.
 *
 * kind is ExprKind::INVOKESTATIC_WITH_EXCEPTION
 *
 */
class InvokeStaticWithException : public Terminator {
    friend class ExprTypeConverter;
    friend class PrivateTypeConverterNoInvokeOriginal;
public:
    /**
     * @brief Retrieves the method name of this Invoke operation.
     *
     * @return The method name of this Invoke operation.
     */
    std::string GetMethodName() const;

    /**
     * @brief Retrieves the method type of this Invoke operation.
     *
     * @return The method type of this Invoke operation.
     */
    FuncType* GetMethodType() const;

    /**
     * @brief Retrieves the type of 'this'.
     *
     * @return The type of 'this'.
     */
    Type* GetThisType() const;

    /**
     * @brief Retrieves the instantiated parent custom type of the callee.
     *
     * @return The instantiated parent custom type of the callee.
     */
    Type* GetInstParentCustomTyOfCallee() const;

    /**
     * @brief Retrieves the instantiated parameter types.
     *
     * @return A vector of pointers to the instantiated parameter types.
     */
    std::vector<Type*> GetInstantiatedParamTypes() const;

    /**
     * @brief Retrieves the instantiated return type.
     *
     * @return The instantiated return type.
     */
    Type* GetInstantiatedRetType() const;

    /**
     * @brief Sets the instantiated function type.
     *
     * @param funcType The function type.
     */
    void SetInstantiatedFuncType(FuncType& funcType);

    /**
     * @brief Retrieves the instantiated type arguments.
     *
     * @return A vector of pointers to the instantiated type arguments.
     */
    std::vector<Type*> GetInstantiatedTypeArgs() const;

    /**
     * @brief Sets the function information.
     *
     * @param info The function information.
     */
    void SetFuncInfo(const InvokeCalleeInfo& info);

    /**
     * @brief Retrieves the function information.
     *
     * @return The function information.
     */
    InvokeCalleeInfo GetFuncInfo() const;

    /**
     * @brief Retrieves the RTTI value.
     *
     * @return The RTTI value.
     */
    Value* GetRTTIValue() const;

    /**
     * @brief Retrieves the call arguments of this InvokeStaticWithException operation.
     *
     * @return A vector of pointers to the call arguments.
     */
    std::vector<Value*> GetArgs() const;

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit InvokeStaticWithException(Value* rtti, const std::vector<Value*>& args, const InvokeCalleeInfo& funcInfo,
        Block* sucBlock, Block* errBlock, Block* parent);

    ~InvokeStaticWithException() override = default;

    friend class CHIRContext;
    friend class CHIRBuilder;

    std::string ToString(unsigned indent = 0) const override;

    InvokeStaticWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    InvokeCalleeInfo funcInfo;
};

/*
 * @brief IntOpWithException class in CHIR.
 *
 * kind is ExprKind::INT_OP_WITH_EXCEPTION
 */
class IntOpWithException : public Terminator {
public:
    /**
     * @brief Retrieves the operation kind.
     *
     * @return The operation kind.
     */
    ExprKind GetOpKind() const;

    /**
     * @brief Retrieves the left-hand side operand.
     *
     * @return The left-hand side operand.
     */
    Value* GetLHSOperand() const;

    /**
     * @brief Retrieves the right-hand side operand.
     *
     * @return The right-hand side operand.
     */
    Value* GetRHSOperand() const;

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

    /**
     * @brief Converts the expression to a string representation.
     *
     * @param indent The indentation level for formatting (default is 0).
     * @return The string representation of the expression.
     */
    std::string ToString(unsigned indent = 0) const override;

    /**
     * @brief Retrieves the overflow strategy.
     *
     * @return The overflow strategy.
     */
    OverflowStrategy GetOverflowStrategy() const;

private:
    explicit IntOpWithException(ExprKind unaryKind, Value* operand, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::INT_OP_WITH_EXCEPTION, {operand}, {normal, exception}, parent), opKind(unaryKind)
    {
    }
    explicit IntOpWithException(
        ExprKind binaryKind, Value* lhs, Value* rhs, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::INT_OP_WITH_EXCEPTION, {lhs, rhs}, {normal, exception}, parent), opKind(binaryKind)
    {
    }
    ~IntOpWithException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    IntOpWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    ExprKind opKind; // Operator Kind
    Cangjie::OverflowStrategy overflowStrategy{Cangjie::OverflowStrategy::NA};
};

/*
 * @brief TypeCastWithException class in CHIR.
 *
 * kind is ExprKind::TYPECAST_WITH_EXCEPTION
 */
class TypeCastWithException : public Terminator {
public:
    /**
     * @brief Retrieves the overflow strategy.
     *
     * @return The overflow strategy.
     */
    OverflowStrategy GetOverflowStrategy() const;

    /**
     * @brief Retrieves the source value of this cast operation.
     *
     * @return The source value of this cast operation.
     */
    Value* GetSourceValue() const;

    /**
     * @brief Retrieves the source type of this cast operation.
     *
     * @return The source type of this cast operation.
     */
    Type* GetSourceTy() const;

    /**
     * @brief Retrieves the target type of this cast operation.
     *
     * @return The target type of this cast operation.
     */
    Type* GetTargetTy() const;

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit TypeCastWithException(Value* operand, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::TYPECAST_WITH_EXCEPTION, {operand}, {normal, exception}, parent)
    {
    }
    ~TypeCastWithException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    TypeCastWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief IntrinsicWithException class in CHIR.
 *
 * kind is ExprKind::INTRINSIC_WITH_EXCEPTION
 */
class IntrinsicWithException : public Terminator {
    friend class ExprTypeConverter;
public:
    /**
     * @brief Retrieves the intrinsic kind.
     *
     * @return The intrinsic kind.
     */
    CHIR::IntrinsicKind GetIntrinsicKind() const;

    /**
     * @brief Converts the expression to a string representation.
     *
     * @param indent The indentation level for formatting (default is 0).
     * @return The string representation of the expression.
     */
    std::string ToString(unsigned indent = 0) const override;

    /**
     * @brief Retrieves the generic type information.
     *
     * @return A vector of pointers to the generic types.
     */
    const std::vector<Ptr<Type>>& GetGenericTypeInfo() const;

    /**
     * @brief Sets the generic type information.
     *
     * @param types A vector of pointers to the generic types.
     */
    void SetGenericTypeInfo(const std::vector<Ptr<Type>>& types);

    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

    /**
     * @brief Retrieves the arguments of the intrinsic operation.
     *
     * @return A vector of pointers to the arguments.
     */
    const std::vector<Value*> GetArgs() const;
private:
    explicit IntrinsicWithException(
        CHIR::IntrinsicKind kind, const std::vector<Value*>& args, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::INTRINSIC_WITH_EXCEPTION, args, {normal, exception}, parent), intrinsicKind(kind)
    {
    }
    ~IntrinsicWithException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    IntrinsicWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    CHIR::IntrinsicKind intrinsicKind; // Intrinsic kind
    std::vector<Ptr<Type>> genericTypeInfo;
};

/*
 * @brief AllocateWithException class in CHIR.
 *
 * kind is ExprKind::ALLOCATE_WITH_EXCEPTION
 */
class AllocateWithException : public Terminator {
    friend class ExprTypeConverter;
public:
    Type* GetType() const;

    Block* GetSuccessBlock() const;

    Block* GetErrorBlock() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit AllocateWithException(Type* ty, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::ALLOCATE_WITH_EXCEPTION, {}, {normal, exception}, parent), ty(ty)
    {
    }
    ~AllocateWithException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    AllocateWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    /** @brief The type to be allocated.
     */
    Type* ty;
};

/*
 * @brief RawArrayAllocateWithException class in CHIR.
 *
 * kind is ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION
 */
class RawArrayAllocateWithException : public Terminator {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
public:
    Value* GetSize() const;

    Type* GetElementType() const;

    void SetElementType(Type& type);

    Block* GetSuccessBlock() const;

    Block* GetErrorBlock() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit RawArrayAllocateWithException(Type* eleTy, Value* size, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION, {size}, {normal, exception}, parent),
          elementType(eleTy)
    {
    }
    ~RawArrayAllocateWithException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    RawArrayAllocateWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    Type* elementType; // The element type.
};

/*
 * @brief SpawnWithException class in CHIR.
 *
 * kind is ExprKind::SPAWN_WITH_EXCEPTION
 */
class SpawnWithException : public Terminator {
public:
    /** @brief Get the future argument for execute.*/
    Value* GetFuture() const;
    /**
     * @brief Get the spawn argument.
     *
     * @return nullopt if no argument.
     */
    std::optional<Value*> GetSpawnArg() const;

    /** @brief Get the closure argument for execute closure.*/
    Value* GetClosure() const;

    /**
     * @brief Get the FuncBase* of execute closure.
     *
     * @return nullptr if not exist.
     */
    FuncBase* GetExecuteClosure() const;

    bool IsExecuteClosure() const;

    Block* GetSuccessBlock() const;

    Block* GetErrorBlock() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit SpawnWithException(
        Value* val, Value* arg, FuncBase* func, bool isClosure, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::SPAWN_WITH_EXCEPTION, {val, arg}, {normal, exception}, parent),
          executeClosure(func),
          isExecuteClosure(isClosure)
    {
    }
    explicit SpawnWithException(
        Value* val, FuncBase* func, bool isClosure, Block* normal, Block* exception, Block* parent)
        : Terminator(ExprKind::SPAWN_WITH_EXCEPTION, {val}, {normal, exception}, parent),
          executeClosure(func),
          isExecuteClosure(isClosure)
    {
    }
    ~SpawnWithException() override
    {
        executeClosure = nullptr;
    }
    friend class CHIRContext;
    friend class CHIRBuilder;

    SpawnWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    /**
     * @brief the FuncBase* of executeClosure func for spawn with closure.
     *
     * usually, the executeClosure's type is Func*, but if spawn is in a generic context,
     * the type will be ImportedFunc*, e.g.
     *
     * func foo<T> (callback: ()->T) {
     *     try{
     *         spawn {
     *             return callback()
     *         }
     *     } catch (...) {...}
     * }
     */
    FuncBase* executeClosure{};
    bool isExecuteClosure{false}; // false for execute, true for execute closure.
};

/*
 * @brief Tuple class in CHIR.
 *
 * kind is ExprKind::TUPLE
 */
class Tuple : public Expression {
public:
    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Tuple(const std::vector<Value*>& values, Block* parent) : Expression(ExprKind::TUPLE, values, {}, parent)
    {
    }
    ~Tuple() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Tuple* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Field class in CHIR.
 *
 * kind is ExprKind::FIELD
 */
class Field : public Expression {
public:
    Value* GetBase() const;

    std::vector<uint64_t> GetIndexes() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    /*
     * @brief the indexes used to store the Int Literal
     */
    std::vector<uint64_t> indexes;

    explicit Field(Value* val, const std::vector<uint64_t>& indexes, Block* parent)
        : Expression(ExprKind::FIELD, {val}, {}, parent), indexes(indexes)
    {
    }
    ~Field() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Field* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief RawArrayAllocate class in CHIR.
 *
 * kind is ExprKind::RAW_ARRAY_ALLOCATE
 */
class RawArrayAllocate : public Expression {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
public:
    Value* GetSize() const;

    Type* GetElementType() const;

    void SetElementType(Type& type);

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit RawArrayAllocate(Type* eleTy, Value* size, Block* parent)
        : Expression(ExprKind::RAW_ARRAY_ALLOCATE, {size}, {}, parent), elementType(eleTy)
    {
    }
    ~RawArrayAllocate() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    RawArrayAllocate* Clone(CHIRBuilder& builder, Block& parent) const override;

    Type* elementType; // The element type.
};

/*
 * @brief RawArrayLiteralInit class in CHIR.
 *
 * kind is ExprKind::RAW_ARRAY_LITERAL_INIT
 */
class RawArrayLiteralInit : public Expression {
public:
    Value* GetRawArray() const;

    size_t GetSize() const;

    std::vector<Value*> GetElements() const;

private:
    explicit RawArrayLiteralInit(const Ptr<Value> raw, std::vector<Value*> elements, Block* parent);
    ~RawArrayLiteralInit() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    RawArrayLiteralInit* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief RawArrayInitByValue class in CHIR.
 *
 * kind is ExprKind::RAW_ARRAY_INIT_BY_VALUE
 */
class RawArrayInitByValue : public Expression {
public:
    Value* GetRawArray() const;

    Value* GetSize() const;

    Value* GetInitValue() const;

private:
    explicit RawArrayInitByValue(Value* raw, Value* size, Value* initVal, Block* parent)
        : Expression(ExprKind::RAW_ARRAY_INIT_BY_VALUE, {raw, size, initVal}, {}, parent)
    {
    }
    ~RawArrayInitByValue() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    RawArrayInitByValue* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief VArray class in CHIR.
 *
 * kind is ExprKind::VARRAY
 */
class VArray : public Expression {
public:
    int64_t GetSize() const;

private:
    explicit VArray(std::vector<Value*> elements, Block* parent) : Expression(ExprKind::VARRAY, elements, {}, parent)
    {
    }
    ~VArray() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    VArray* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief VArrayBuilder class in CHIR.
 *
 * kind is ExprKind::VARRAY_BUILDER
 */
class VArrayBuilder : public Expression {
public:
    Value* GetSize() const;

    Value* GetItem() const;

    Value* GetInitFunc() const;

private:
    explicit VArrayBuilder(Value* size, Value* item, Value* initFunc, Block* parent)
        : Expression(ExprKind::VARRAY_BUILDER, {size, item, initFunc}, {}, parent)
    {
    }
    enum class ElementIdx { SIZE_IDX = 0, ITEM_IDX = 1, INIT_FUNC_IDX = 2 };
    ~VArrayBuilder() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    VArrayBuilder* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief GetException class in CHIR.
 *
 * kind is ExprKind::GET_EXCEPTION
 */
class GetException : public Expression {
private:
    explicit GetException(Block* parent) : Expression(ExprKind::GET_EXCEPTION, {}, {}, parent)
    {
    }
    explicit GetException(const GetException& other, Block* parent) : Expression(other, parent)
    {
    }
    ~GetException() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    GetException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Intrinsic class in CHIR.
 *
 * kind is ExprKind::INTRINSIC
 */
class Intrinsic : public Expression {
    friend class ExprTypeConverter;
public:
    /**
     * @brief Retrieves the intrinsic kind.
     *
     * @return The intrinsic kind.
     */
    CHIR::IntrinsicKind GetIntrinsicKind() const;

    /**
     * @brief Converts the expression to a string representation.
     *
     * @param indent The indentation level for formatting (default is 0).
     * @return The string representation of the expression.
     */
    std::string ToString(unsigned indent = 0) const override;

    /**
     * @brief Retrieves the generic type information.
     *
     * @return A vector of pointers to the generic types.
     */
    const std::vector<Ptr<Type>>& GetGenericTypeInfo() const;

    /**
     * @brief Sets the generic type information.
     *
     * @param types A vector of pointers to the generic types.
     */
    void SetGenericTypeInfo(const std::vector<Ptr<Type>>& types);

    /**
     * @brief Retrieves the arguments of the intrinsic operation.
     *
     * @return A vector of pointers to the arguments.
     */
    const std::vector<Value*>& GetArgs() const;
private:
    explicit Intrinsic(CHIR::IntrinsicKind kind, const std::vector<Value*>& args, Block* parent)
        : Expression(ExprKind::INTRINSIC, args, {}, parent), intrinsicKind(kind)
    {
    }
    ~Intrinsic() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Intrinsic* Clone(CHIRBuilder& builder, Block& parent) const override;

private:
    CHIR::IntrinsicKind intrinsicKind;     // Intrinsic kind
    std::vector<Ptr<Type>> genericTypeInfo; // Intrinsic genericType used in CodeGen
};

/*
 * @brief If class in CHIR.
 *
 * If is an structed control flow expression with two block groups.
 */
class If : public Expression {
public:
    /** @brief Get the condition of this If Expression */
    Value* GetCondition() const;
    /** @brief Get the true branch of this If Expression */
    BlockGroup* GetTrueBranch() const;
    /** @brief Get the false branch of this If Expression */
    BlockGroup* GetFalseBranch() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit If(Value* cond, BlockGroup* thenBody, BlockGroup* elseBody, Block* parent)
        : Expression(ExprKind::IF, {cond}, {thenBody, elseBody}, parent)
    {
    }

    ~If() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    If* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Loop class in CHIR.
 *
 * Loop is an structed control flow expression with a block group.
 */
class Loop : public Expression {
public:
    /** @brief Get the loop body of this Loop Expression */
    BlockGroup* GetLoopBody() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Loop(BlockGroup* loopBody, Block* parent) : Expression(ExprKind::LOOP, {}, {loopBody}, parent)
    {
    }
    ~Loop() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Loop* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief ForIn base class in CHIR.
 *
 * ForIn is an structed control flow expression with three block groups.
 */
class ForIn : public Expression {
public:
    /** @brief Get the induction variable of this ForIn Expression */
    Value* GetInductionVar() const;

    /** @brief Get the loop condition variable of this ForIn Expression */
    Value* GetLoopCondVar() const;

    /** @brief Get the body block group of this ForIn Expression */
    BlockGroup* GetBody() const;

    /** @brief Get the latch block group of this ForIn Expression */
    BlockGroup* GetLatch() const;

    /** @brief Get the conditional block group of this ForIn Expression */
    BlockGroup* GetCond() const;

    std::string ToString(unsigned indent = 0) const override;

    /// Describes the execution order of sub block groups in a forin expression
    struct BGExecutionOrder {
        BGExecutionOrder(std::initializer_list<BlockGroup*> bgs)
        {
            CJC_ASSERT(bgs.size() == BG_NUMBER);
            std::copy(bgs.begin(), bgs.end(), s);
        }

        BlockGroup* const * begin() const
        {
            return &s[0];
        }
        BlockGroup* const * end() const
        {
            return s + BG_NUMBER;
        }
    private:
        constexpr static int BG_NUMBER{3};
        BlockGroup* s[BG_NUMBER];
    };
    /// Get the execution order of a forin expression. The first block is the jump target from outer expressions.
    virtual BGExecutionOrder GetExecutionOrder() const = 0;

protected:
    explicit ForIn(ExprKind kind,
        Value* inductionVar, Value* loopCondVar, BlockGroup* body, BlockGroup* latch, BlockGroup* cond, Block* parent)
        : Expression(kind, {inductionVar, loopCondVar}, {body, latch, cond}, parent)
    {
    }

    ~ForIn() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    template <class T> T* CloneBase(CHIRBuilder& builder, Block& parent) const;
};

/*
 * @brief ForInRange is expression translated from AST for-in-range or for-in-iter. The BG execution order is
 * cond->body->latch
 */
class ForInRange : public ForIn {
public:
    ForInRange(Value* inductionVar, Value* loopCondVar, BlockGroup* body, BlockGroup* latch, BlockGroup* cond,
        Block* parent) : ForIn(ExprKind::FORIN_RANGE, inductionVar, loopCondVar, body, latch, cond, parent) {}

    BGExecutionOrder GetExecutionOrder() const override;
private:
    ForInRange* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief ForInRange is expression translated from AST for-in-range or for-in-iter. The BG execution order is
 * latch->cond->body
 */
class ForInIter : public ForIn {
public:
    ForInIter(Value* inductionVar, Value* loopCondVar, BlockGroup* body, BlockGroup* latch, BlockGroup* cond,
        Block* parent) : ForIn(ExprKind::FORIN_ITER, inductionVar, loopCondVar, body, latch, cond, parent) {}

    BGExecutionOrder GetExecutionOrder() const override;
private:
    ForInIter* Clone(CHIRBuilder& builder, Block& parent) const override;
};

class ForInClosedRange : public ForIn {
public:
    ForInClosedRange(Value* inductionVar, Value* loopCondVar, BlockGroup* body, BlockGroup* latch, BlockGroup* cond,
        Block* parent) : ForIn(ExprKind::FORIN_CLOSED_RANGE, inductionVar, loopCondVar, body, latch, cond, parent) {}

    BGExecutionOrder GetExecutionOrder() const override;
private:
    ForInClosedRange* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/*
 * @brief Lambda class in CHIR.
 *
 * Lambda is a expression of lambda or nested function with a block group.
 */
class Lambda : public Expression {
    friend class ExprTypeConverter;
public:
    /**
     * @brief Retrieves the lambda body of this Lambda Expression.
     *
     * @return The lambda body of this Lambda Expression.
     */
    BlockGroup* GetLambdaBody() const;

    /**
     * @brief Appends a block group to the lambda body.
     *
     * @param group The block group to append.
     */
    void AppendBlockGroup(BlockGroup& group);

    /**
     * @brief Initializes the body of the lambda expression.
     *
     * @param newBody The new body to initialize.
     */
    void InitBody(BlockGroup& newBody);

    /**
     * @brief Retrieves the function type of the lambda expression.
     *
     * @return The function type of the lambda expression.
     */
    FuncType* GetFuncType() const;

    /**
     * @brief Retrieves the return type of the lambda expression.
     *
     * @return The return type of the lambda expression.
     */
    Type* GetReturnType() const;

    /** @brief Get the lambda body of this Lambda Expression */
    BlockGroup* GetBody() const;

    /**
     * @brief Removes the body of the lambda expression.
     */
    void RemoveBody();

    /**
     * @brief Retrieves the entry block of the lambda expression.
     *
     * @return The entry block of the lambda expression.
     */
    Block* GetEntryBlock() const;

    /**
     * @brief Retrieves the identifier of the lambda expression.
     *
     * @return The identifier of the lambda expression.
     */
    std::string GetIdentifier() const;

    /**
     * @brief Retrieves the identifier of the lambda expression without the prefix '@'.
     *
     * @return The identifier of the lambda expression without the prefix '@'.
     */
    std::string GetIdentifierWithoutPrefix() const;

    /**
     * @brief Retrieves the source code identifier of the lambda expression.
     *
     * @return The source code identifier of the lambda expression.
     */
    std::string GetSrcCodeIdentifier() const;

    /**
     * @brief Adds a parameter to the lambda expression.
     *
     * @param arg The parameter to add.
     */
    void AddParam(Parameter& arg);

    /**
     * @brief Retrieves the number of parameters in the lambda expression.
     *
     * @return The number of parameters in the lambda expression.
     */
    size_t GetNumOfParams() const;

    /**
     * @brief Retrieves a parameter at a specific index.
     *
     * @param index The index of the parameter.
     * @return The parameter at the specified index.
     */
    Parameter* GetParam(size_t index) const;

    /**
     * @brief Retrieves all parameters of the lambda expression.
     *
     * @return A vector of pointers to the parameters.
     */
    const std::vector<Parameter*>& GetParams() const;

    /**
     * @brief Retrieves the generic type parameters of the lambda expression.
     *
     * @return A vector of pointers to the generic type parameters.
     */
    const std::vector<GenericType*>& GetGenericTypeParams() const;

    /**
     * @brief Sets the return value of the lambda expression.
     *
     * @param ret The return value to set.
     */
    void SetReturnValue(LocalVar& ret);

    /**
     * @brief Retrieves the return value of the lambda expression.
     *
     * @return A pointer to the return value.
     */
    LocalVar* GetReturnValue() const;

    /**
     * @brief Checks if the lambda expression is local.
     *
     * @return True if the lambda expression is local, false otherwise.
     */
    bool IsLocalFunc() const;

    /**
     * @brief Sets the host function for parameter default values.
     *
     * @param hostFunc The host function to set.
     */
    void SetParamDftValHostFunc(Lambda& hostFunc);

    /**
     * @brief Retrieves the host function for parameter default values.
     *
     * @return The host function for parameter default values.
     */
    Lambda* GetParamDftValHostFunc() const;

    /**
     * @brief Sets the captured variables of the lambda expression.
     *
     * @param vars The captured variables to set.
     */
    void SetCapturedVars(const std::vector<Value*>& vars);

    /**
     * @brief Retrieves the captured variables of the lambda expression.
     *
     * @return A vector of pointers to the captured variables.
     */
    const std::vector<Value*>& GetCapturedVars() const;

    /**
     * @brief Retrieves the environment variables of the lambda expression.
     *
     * @return A vector of pointers to the environment variables.
     */
    std::vector<Value*> GetEnvironmentVars() const;

    /**
     * @brief Removes the lambda expression from its block.
     */
    void RemoveSelfFromBlock() override;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Lambda(FuncType* ty, Block* parent, bool isLocalFunc = false, const std::string& identifier = "",
        const std::string& srcCodeIdentifier = "", const std::vector<GenericType*>& genericTypeParams = {});
    ~Lambda() override
    {
        funcTy = nullptr;
    };

    friend class CHIRContext;
    friend class CHIRBuilder;

    Lambda* Clone(CHIRBuilder& builder, Block& parent) const override;

private:
    std::string identifier;        // the mangledName of nested function or lambda.
    std::string srcCodeIdentifier; // the name of nested function or lambda.
    FuncBody body;
    FuncType* funcTy;
    bool isLocalFunc{false};
    std::vector<Value*> capturedVars;
    std::vector<GenericType*> genericTypeParams;
    Lambda* paramDftValHostFunc = nullptr;
};

/*
 * @brief Debug class in CHIR.
 *
 * Debug is a debug expression for bind a local mangledName into user-defined name and debug location info.
 */
class Debug : public Expression {
public:
    /**
     * @brief obtains the identifier of the corresponding Cangjie source code.
     */
    std::string GetSrcCodeIdentifier() const;

    Value* GetValue() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Debug(Value* local, std::string srcCodeIdentifier, Block* parent);
    ~Debug() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    Debug* Clone(CHIRBuilder& builder, Block& parent) const override;
    // The variable name user-defined
    std::string srcCodeIdentifier;

    // Record accruate ty if this is a env obj for closure-converted func case. This
    // information is needed by CJDB
    Type* accurateEnvObjTy = nullptr;
};

class Spawn : public Expression {
public:
    /** @brief Get the future argument for execute.*/
    Value* GetFuture() const;

    /**
     * @brief Get the spawn argument.
     *
     * @return nullopt if no argument.
     */
    std::optional<Value*> GetSpawnArg() const;

    /** @brief Get the closure argument for execute closure.*/
    Value* GetClosure() const;

    /**
     * @brief Get the FuncBase* of execute closure.
     *
     * @return nullptr if not exist.
     */
    FuncBase* GetExecuteClosure() const;

    bool IsExecuteClosure() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit Spawn(Value* val, FuncBase* func, bool isClosure, Block* parent)
        : Expression(ExprKind::SPAWN, {val}, {}, parent), executeClosure(func), isExecuteClosure(isClosure)
    {
    }
    explicit Spawn(Value* val, Value* arg, FuncBase* func, bool isClosure, Block* parent)
        : Expression(ExprKind::SPAWN, {val, arg}, {}, parent), executeClosure(func), isExecuteClosure(isClosure)
    {
    }
    void SetExecuteClosure(bool newIsExecuteClosure);
    ~Spawn() override
    {
        executeClosure = nullptr;
    }
    friend class CHIRContext;
    friend class CHIRBuilder;
    friend class RedundantFutureRemoval;

    Spawn* Clone(CHIRBuilder& builder, Block& parent) const override;

private:
    /**
     * @brief the FuncBase* of executeClosure func for spawn with closure.
     *
     * usually, the executeClosure's type is Func*, but if spawn is in a generic context,
     * the type will be ImportedFunc*, e.g.
     *
     * func foo<T> (callback: ()->T) {
     *     spawn {
     *         return callback()
     *     }
     * }
     *
     */
    FuncBase* executeClosure{};

    /**
     * @brief indicate runtime should use \p executeClosure to execute this \p Spawn.
     *
     * There are two ways to execute a spawn, correspond two Future's interface in std/core, which is called by runtime,
     * the two interface are: \p execute and \p executeClosure, depends on whether the spawn's result is used,
     * if a spawn's result is used, the spawn is executed by \p execute, otherwise, to reduce performance overhead,
     * is executed by \p executeClosure, which the runtime has some optimization on.
     */
    bool isExecuteClosure{false};
};

class GetInstantiateValue : public Expression {
    friend class ExprTypeConverter;
public:
    std::vector<Type*> GetInstantiateTypes() const;

    Value* GetGenericResult() const;

    std::string ToString(unsigned indent = 0) const override;

private:
    explicit GetInstantiateValue(Value* val, std::vector<Type*> insTypes, Block* parent)
        : Expression(ExprKind::GET_INSTANTIATE_VALUE, {val}, {}, parent), instantiateTys(insTypes)
    {
    }
    ~GetInstantiateValue() override = default;
    friend class CHIRContext;
    friend class CHIRBuilder;

    std::vector<Type*> instantiateTys;
    GetInstantiateValue* Clone(CHIRBuilder& builder, Block& parent) const override;
};

const std::unordered_map<Cangjie::OverflowStrategy, std::string> OVERFLOW_TO_STRING_MAP{
    {Cangjie::OverflowStrategy::NA, "NA"},
    {Cangjie::OverflowStrategy::CHECKED, "CHECKED"},
    {Cangjie::OverflowStrategy::WRAPPING, "WRAPPING"},
    {Cangjie::OverflowStrategy::THROWING, "THROWING"},
    {Cangjie::OverflowStrategy::SATURATING, "SATURATING"},
};
} // namespace Cangjie::CHIR
#endif // CANGJIE_CHIR_EXPRESSION_H
