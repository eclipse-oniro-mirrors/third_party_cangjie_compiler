// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_PRIVATE_TYPE_CONVERTER_H
#define CANGJIE_CHIR_PRIVATE_TYPE_CONVERTER_H

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/Type/ClassDef.h"
#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/Type/EnumDef.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/Type/StructDef.h"

namespace Cangjie::CHIR {
template <typename FType>
class ExprTypeFunctor;

template <typename FType>
class ValueTypeFunctor;

template <typename FType>
class CustomDefTypeFunctor;

#define VISIT_IMPL_DEFAULT(OP_TYPE) \
{ \
    return Visit##OP_TYPE##DefaultImpl(o, std::forward<Args>(args)...); \
}

#define VISIT_IMPL_DISPATCH_EXPR(KIND, OP) \
{ \
    KIND, \
    [](TSelf* self, Expression& o, Args...args) { \
        return self->VisitSubExpression(*StaticCast<OP*>(&o), std::forward(args)...); \
    } \
}

#define VISIT_IMPL_DISPATCH_VALUE(KIND, OP) \
{ \
    KIND, \
    [](TSelf* self, Value& o, Args...args) { \
        return self->VisitSubValue(*VirtualCast<OP*>(&o), std::forward(args)...); \
    } \
}

#define VISIT_IMPL_DISPATCH_DEF(KIND, OP) \
{ \
    KIND, \
    [](TSelf* self, CustomTypeDef& o, Args...args) { \
        return self->VisitSubDef(*StaticCast<OP*>(&o), std::forward(args)...); \
    } \
}

template <typename R, typename... Args>
class CustomDefTypeFunctor<R(CustomTypeDef& o, Args...)> {
public:
    using TSelf = CustomDefTypeFunctor<R(CustomTypeDef& o, Args...)>;
    using FType = std::function<R(TSelf* self, CustomTypeDef& o, Args...)>;
    using Dispatcher = std::unordered_map<CustomDefKind, FType>;
    virtual ~CustomDefTypeFunctor()
    {
    }
    virtual R VisitDef(CustomTypeDef& o, Args... args)
    {
        static Dispatcher dispatcher = InitCustomTypeDefTable();
        if (auto func = dispatcher.find(o.GetCustomKind()); func != dispatcher.end()) {
            return func->second(this, o, std::forward<args>...);
        }
        return VisitDefDefaultImpl(o, std::forward<args>...);
    }

protected:
    virtual R VisitDefDefaultImpl([[maybe_unused]] CustomTypeDef& o, [[maybe_unused]] Args... args)
    {
        CJC_ABORT();
    }
    virtual R VisitSubDef(StructDef& o, Args... args) VISIT_IMPL_DEFAULT(Def);
    virtual R VisitSubDef(EnumDef& o, Args... args) VISIT_IMPL_DEFAULT(Def);
    virtual R VisitSubDef(ClassDef& o, Args... args) VISIT_IMPL_DEFAULT(Def);
    virtual R VisitSubDef(ExtendDef& o, Args... args) VISIT_IMPL_DEFAULT(Def);

private:
    static Dispatcher InitCustomTypeDefTable()
    {
        Dispatcher dispatcher = {
            VISIT_IMPL_DISPATCH_DEF(CustomDefKind::TYPE_STRUCT, StructDef),
            VISIT_IMPL_DISPATCH_DEF(CustomDefKind::TYPE_ENUM, EnumDef),
            VISIT_IMPL_DISPATCH_DEF(CustomDefKind::TYPE_CLASS, ClassDef),
            VISIT_IMPL_DISPATCH_DEF(CustomDefKind::TYPE_EXTEND, ExtendDef),
        };
        return dispatcher;
    }
};

template <typename R, typename... Args>
class ExprTypeFunctor<R(Expression& o, Args...)> {
public:
    using TSelf = ExprTypeFunctor<R(Expression& o, Args...)>;
    using FType = std::function<R(TSelf* self, Expression& o, Args...)>;
    using Dispatcher = std::unordered_map<ExprKind, FType>;
    virtual ~ExprTypeFunctor()
    {
    }
    virtual R VisitExpr(Expression& o, Args... args)
    {
        static Dispatcher dispatcher = InitExprVTable();
        if (auto func = dispatcher.find(o.GetExprKind()); func != dispatcher.end()) {
            return dispatcher.at(o.GetExprKind())(this, o, std::forward<args>...);
        }
        return VisitExprDefaultImpl(o, std::forward<args>...);
    }

protected:
    virtual R VisitExprDefaultImpl([[maybe_unused]] Expression& o, [[maybe_unused]] Args... args)
    {
        CJC_ABORT();
    }
    virtual R VisitSubExpression(Allocate& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(AllocateWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(InstanceOf& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(RawArrayAllocate& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(RawArrayAllocateWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(Apply& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(ApplyWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(Invoke& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(InvokeWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(InvokeStatic& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(InvokeStaticWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(Constant& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(Intrinsic& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(IntrinsicWithException& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(GetInstantiateValue& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(Lambda& o, Args... args) VISIT_IMPL_DEFAULT(Expr);
    virtual R VisitSubExpression(GetRTTIStatic& o, Args... args) VISIT_IMPL_DEFAULT(Expr);

private:
    static Dispatcher InitExprVTable()
    {
        Dispatcher dispatcher = {
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::ALLOCATE, Allocate),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::ALLOCATE_WITH_EXCEPTION, AllocateWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INSTANCEOF, InstanceOf),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::RAW_ARRAY_ALLOCATE, RawArrayAllocate),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION, RawArrayAllocateWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::APPLY, Apply),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::APPLY_WITH_EXCEPTION, ApplyWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INVOKE, Invoke),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INVOKE_WITH_EXCEPTION, InvokeWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INVOKESTATIC, InvokeStatic),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INVOKESTATIC_WITH_EXCEPTION, InvokeStaticWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::CONSTANT, Constant),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INTRINSIC, Intrinsic),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::INTRINSIC_WITH_EXCEPTION, IntrinsicWithException),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::GET_INSTANTIATE_VALUE, GetInstantiateValue),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::LAMBDA, Lambda),
            VISIT_IMPL_DISPATCH_EXPR(ExprKind::GET_RTTI_STATIC, GetRTTIStatic),
        };
        return dispatcher;
    }
};

template <typename R, typename... Args>
class ValueTypeFunctor<R(Value& o, Args...)> {
public:
    using TSelf = ValueTypeFunctor<R(Value& o, Args...)>;
    using FType = std::function<R(TSelf* self, Value& o, Args...)>;
    using Dispatcher = std::unordered_map<Value::ValueKind, FType>;
    virtual ~ValueTypeFunctor()
    {
    }
    virtual R VisitValue(Value& o, Args... args)
    {
        static Dispatcher dispatcher = InitValueVTable();
        if (auto func = dispatcher.find(o.GetValueKind()); func != dispatcher.end()) {
            return func->second(this, o, std::forward<args>...);
        }
        return VisitValueDefaultImpl(o, std::forward<args>...);
    }

protected:
    virtual R VisitValueDefaultImpl([[maybe_unused]] Value& o, [[maybe_unused]] Args... args)
    {
        CJC_ABORT();
    }
    virtual R VisitSubValue(Func& o, Args... args) VISIT_IMPL_DEFAULT(Value);
    virtual R VisitSubValue(ImportedFunc& o, Args... args) VISIT_IMPL_DEFAULT(Value);

private:
    static Dispatcher InitValueVTable()
    {
        Dispatcher dispatcher = {
            VISIT_IMPL_DISPATCH_VALUE(Value::ValueKind::KIND_FUNC, Func),
            VISIT_IMPL_DISPATCH_VALUE(Value::ValueKind::KIND_IMP_FUNC, ImportedFunc),
        };
        return dispatcher;
    }
};

#undef VISIT_IMPL_DEFAULT
#undef VISIT_IMPL_DISPATCH_EXPR
#undef VISIT_IMPL_DISPATCH_VALUE

class TypeConverter {
public:
    TypeConverter(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : converter(converter), builder(builder)
    {
    }
    virtual ~TypeConverter() = default;

protected:
    virtual Type* ConvertType(Type& type);
    FuncType* ConvertFuncParamsAndRetType(const FuncType& input);

    ConvertTypeFunc converter;
    CHIRBuilder& builder;
};

class ExprTypeConverter : public virtual TypeConverter, public ExprTypeFunctor<void(Expression& n)> {
public:
    ExprTypeConverter(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : TypeConverter(converter, builder)
    {
    }
    virtual void VisitValue(Value&)
    {
    }

protected:
    void VisitExprDefaultImpl(Expression& o) override;
    void VisitSubExpression(Allocate& o) override;
    void VisitSubExpression(AllocateWithException& o) override;
    void VisitSubExpression(InstanceOf& o) override;
    void VisitSubExpression(RawArrayAllocate& o) override;
    void VisitSubExpression(RawArrayAllocateWithException& o) override;
    void VisitSubExpression(Apply& o) override;
    void VisitSubExpression(ApplyWithException& o) override;
    void VisitSubExpression(Invoke& o) override;
    void VisitSubExpression(InvokeWithException& o) override;
    void VisitSubExpression(InvokeStatic& o) override;
    void VisitSubExpression(InvokeStaticWithException& o) override;
    void VisitSubExpression(Constant& o) override;
    void VisitSubExpression(Intrinsic& o) override;
    void VisitSubExpression(IntrinsicWithException& o) override;
    void VisitSubExpression(GetInstantiateValue& o) override;
    void VisitSubExpression(Lambda& o) override;
    void VisitSubExpression(GetRTTIStatic& o) override;
};

class ValueTypeConverter : public virtual TypeConverter, public ValueTypeFunctor<void(Value& o)> {
public:
    ValueTypeConverter(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : TypeConverter(converter, builder)
    {
    }

    void VisitSubValue(Func& o) override;
    void VisitSubValue(ImportedFunc& o) override;
    void VisitValueDefaultImpl(Value& o) override;
};

class CustomDefTypeConverter : public virtual TypeConverter, public CustomDefTypeFunctor<void(CustomTypeDef& o)> {
public:
    CustomDefTypeConverter(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : TypeConverter(converter, builder)
    {
    }

protected:
    void VisitDefDefaultImpl(CustomTypeDef& o) override;
    void VisitSubDef(StructDef& o) override;
    void VisitSubDef(EnumDef& o) override;
    void VisitSubDef(ClassDef& o) override;
    void VisitSubDef(ExtendDef& o) override;
};

class PrivateTypeConverter : public ExprTypeConverter, public ValueTypeConverter {
public:
    PrivateTypeConverter(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : TypeConverter(converter, builder),
          ExprTypeConverter(converter, builder), ValueTypeConverter(converter, builder)
    {
    }
    using ExprTypeConverter::VisitExpr;
    using ValueTypeConverter::VisitValue;

    void VisitValue(Value& o) override
    {
        return ValueTypeConverter::VisitValue(o);
    }
};

class PrivateTypeConverterNoInvokeOriginal : public PrivateTypeConverter {
public:
    PrivateTypeConverterNoInvokeOriginal(const ConvertTypeFunc& converter, CHIRBuilder& builder)
        : TypeConverter(converter, builder),
          PrivateTypeConverter(converter, builder)
    {
    }

private:
    void VisitSubExpression(Invoke& o) override;
    void VisitSubExpression(InvokeWithException& o) override;
    void VisitSubExpression(InvokeStatic& o) override;
    void VisitSubExpression(InvokeStaticWithException& o) override;
};

class TypeConverterForCC : public ExprTypeConverter, public ValueTypeConverter, public CustomDefTypeConverter {
public:
    TypeConverterForCC(
        const ConvertTypeFunc& normalConverter, const ConvertTypeFunc& funcConverter, CHIRBuilder& builder)
        : TypeConverter(normalConverter, builder),
          ExprTypeConverter(normalConverter, builder),
          ValueTypeConverter(normalConverter, builder),
          CustomDefTypeConverter(normalConverter, builder),
          funcConverter(funcConverter)
    {
    }
    using ExprTypeConverter::VisitExpr;
    using ValueTypeConverter::VisitValue;

    void VisitValue(Value& o) override
    {
        return ValueTypeConverter::VisitValue(o);
    }

protected:
    void VisitSubExpression(RawArrayAllocate& o) override;
    void VisitSubExpression(RawArrayAllocateWithException& o) override;

    Type* ConvertType(Type& type) override;

private:
    ConvertTypeFunc funcConverter;
};
} // namespace Cangjie::CHIR

#endif