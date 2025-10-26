// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares core support for java mirror and mirror subtype
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER

#include "InteropLibBridge.h"
#include "Utils.h"

#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::Java {
using namespace AST;

const std::string JAVA_ARRAY_GET_FOR_REF_TYPES = "$javaarrayget";
const std::string JAVA_ARRAY_SET_FOR_REF_TYPES = "$javaarrayset";
const std::string JAVA_IMPL_ENTITY_ARG_NAME_IN_GENERATED_CTOR = "$obj";

enum class DesugarJavaMirrorImplStage : uint8_t {
    BEGIN,
    MIRROR_GENERATE_STUB,
    MIRROR_GENERATE,
    IMPL_GENERATE,
    MIRROR_DESUGAR,
    IMPL_DESUGAR,
    TYPECHECKS,
    END
};

enum class DesugarCJImplStage : uint8_t { BEGIN, IMPL_GENERATE, IMPL_DESUGAR, TYPECHECKS, END };

class JavaDesugarManager {
public:
    JavaDesugarManager(ImportManager& importManager, TypeManager& typeManager, DiagnosticEngine& diag,
                       const BaseMangler& mangler, const std::optional<std::string>& javaCodeGenPath,
                       const std::string& outputLibPath)
        : importManager(importManager),
          typeManager(typeManager),
          utils(importManager, typeManager),
          diag(diag),
          mangler(mangler),
          lib(importManager, typeManager, diag, utils),
          javaCodeGenPath(javaCodeGenPath),
          outputLibPath(outputLibPath)
    {
            lib.CheckInteropLibVersion();
    }

    /**
     * Stage 1: for each mirror interface and abstract class, generate a class wich implements
     * this interface/abstract class.
     * Such classes are used as wrappers when interface or abstract class type object is returned from java
     */
    void GenerateSyntheticClass(File& file);

    /**
     * Stage 2-3: constructors generation and javaref field insertion.
     * The first step: generate members in `JObject` and insert empty constructor in other mirrors. ([doStub] = `false`)
     * The second step: fill pregenerated bodies ([doStub] = `false`)
     */
    void GenerateInMirrors(File& file, bool doStub);

    void GenerateInMirror(ClassDecl& classDecl, bool doStub);

    /**
     * Stage 4: desugar constructors, methods, etc
     */
    void DesugarMirrors(File& file);

    /**
     * Stage 5: generate constructors and native init/deinit/method call functions (callable from java) for @JavaImpl
     */
    void GenerateInJavaImpls(File& file);

    /**
     * Stage 6: desugar in @JavaImpl
     */
    void DesugarInJavaImpls(File& file);

    /**
     * Stage 7: desugar `as`, `is` where type operand is Java class
     */
    void DesugarTypechecks(File& file);

    void DesugarJavaMirror(ClassDecl& mirror);
    /**
     * for interfaces and abstract classes
     */

    void DesugarJavaMirror(ClassLikeDecl& mirror);
    void DesugarJavaImpl(ClassDecl& jimpl);
    void ProcessJavaMirrorImplStage(DesugarJavaMirrorImplStage stage, File& file);
    void ProcessCJImplStage(DesugarCJImplStage stage, File& file);

    /**
     * Stage 1: generate constructors and native init/deinit/method call functions (callable from java) for CJMapping
     * data structure
     */
    void GenerateInCJMapping(File& file);

    /**
     * Stage 2: desugar in CJMapping data structure
     */
    void DesugarInCJMapping(File& file);

private:
    /**
     * Inserts javaref decl to the class decl:
     *
     * let javaref: Java_CFFI_JavaEntity
     */
    void InsertJavaRefVarDecl(ClassDecl& decl);

    void InsertArrayJavaEntityGet(ClassDecl& decl);
    void InsertArrayJavaEntitySet(ClassDecl& decl);

    /**
     * Generates and inserts constructor in java mirror:
     *
     * public init($ref: Java_CFFI_JavaEntity) {
     *     this.javaref = ref // for JObject
     *     // super($ref) // for other mirrors
     * }
     */
    void InsertJavaMirrorCtor(ClassDecl& decl, bool doStub);

    /**
     * Generates and inserts javaref getter as javaref field will be in synthetic class
     * that implements current interface.
     *
     * abstract getter
     * public func $getJavaRef(): Java_CFFI_JavaEntity
     */
    void InsertAbstractJavaRefGetter(ClassLikeDecl& decl);

    /**
     * public override func $getJavaRef(): Java_CFFI_JavaEntity {
     *     return $javaref
     * }
     */
    void InsertJavaRefGetterWithBody(ClassDecl& decl);

    /**
     * before [ctor]:
     *   init(a1: A, a2: B, ..., an: N) { // empty body generated on the previous compilation steps
     *   }
     * --------
     * after:
     *   init(a1: A, a2: B, ..., an: N) {
     *       this({
     *           let jniEnv = Java_CFFI_get_env()
     *           Java_CFFI_newJavaObject(jniEnv, typeSignature, "(<argsSignature>)V", [
     *               Java_CFFI_JavaEntity(a1),
     *               Java_CFFI_JavaEntity(a2),
     *               ...,
     *               Java_CFFI_JavaEntity(an)])
     *       })
     *   }
     */
    void DesugarJavaMirrorConstructor(FuncDecl& ctor, FuncDecl& generatedCtor);

    /**
     * This constructor could be called from cangjie
     * before [ctor]:
     *   init(a1: A, a2: B, ..., an: N) {
     *       <super(...)> // optional
     *       ...body...
     *   }
     * --------------------------------
     * after:
     *   init(a1: A, a2: B, ..., an: N) {
     *       super({
     *           let jniEnv = Java_CFFI_get_env()
     *           Java_CFFI_newGlobalReference(
     *               jniEnv,
     *               Java_CFFI_newJavaObject(jniEnv, typeSignature, "(<argsSignature>)V", [
     *                   Java_CFFI_JavaEntity(a1),
     *                   Java_CFFI_JavaEntity(a2),
     *                   ...,
     *                   Java_CFFI_JavaEntity(an),
     *                   Java_CFFI_JavaEntity(null) // mark argument for generated java constructor
     *                                              // of Type $$NativeConstructorMarker
     *                   ]),
     *               true // isWeak
     *           )
     *       }) // @JavaMirror constructor call
     *       <super(...)> // optional <- will be removed after java code generation
     *       ...body...
     *   }
     */
    void DesugarJavaImplConstructor(FuncDecl& ctor, FuncDecl& parentCtor);

    /**
     * Create native func
     * public @C func ${funcName} (${params}): retTy {
     *     ${nodes}
     * }
     */
    OwnedPtr<FuncDecl> CreateNativeFunc(const std::string& funcName,
        std::vector<OwnedPtr<FuncParam>> params, Ptr<Ty> retTy, std::vector<OwnedPtr<Node>> nodes);
    /**
     * Unwrap a refExpr of native func param into Cangjie type.
     * ref: jobject -> @JavaMirror/@JavaImpl
     */
    OwnedPtr<Expr> UnwrapRefExpr(OwnedPtr<RefExpr> ref, Ptr<Ty> targetTy, const ClassLikeDecl& decl);
    /**
     * Wrap the cangjie expr into native type.
     * expr: @JavaMirror/@JavaImpl -> jobject
     */
    OwnedPtr<Expr> WrapExprWithExceptionHandling(
        std::vector<OwnedPtr<Node>>&& nodes, OwnedPtr<Expr> expr, FuncParam& env, const ClassLikeDecl& decl);
    /**
     * Generate native for arg:
     * public @C func Java_xxx_superC${ctorId}A${argId}P${paramIds} (${usingParams}) {
     *     return withExceptionHandling(env, { =>
     *         let tmp0 = wrap($usingParams)
     *         let tmp1 = ${clonedExpr($arg)}
     *         return unwrap(tmp)
     *     })
     * }
     */
    OwnedPtr<FuncDecl> GenerateNativeFunc4Argument(const FuncArg& arg,
        const std::vector<OwnedPtr<FuncParam>>& params, ClassLikeDecl& decl, size_t ctorId, size_t argId);
    // Desugar super call in constructor
    OwnedPtr<CallExpr> DesugarJavaImplSuperCall(const FuncDecl& ctor, Decl& jniEnvVar);

    /**
     * This contructor could be called from java
     * for constructor [sampleCtor]:
     *   init(a1: A, a2: B, ..., an: N) { // or primary constructor
     *       <super(...)> // optional
     *       ...body...
     *   }
     *
     * the following will be generated:
     *     init(obj: Java_CFFI_JavaEntity, a1: A, a2: B, ..., an: N) {
     *         super(obj)
     *         ...body...
     *     }
     */
    OwnedPtr<FuncDecl> GenerateJavaImplConstructor(FuncDecl& sampleCtor, ClassLikeDecl& parent);

    /**
     * This generated method could be called from java
     * for method [sampleMethod] of class package.I:
     *
     * func foo(a1: A, a2: B, ..., an: N): Ret {
     *     ...body...
     * }
     *
     * the following native function will be generated:
     *
     * @C
     * func java_package_I_fooImpl{mangling}(env: JNIEnv_ptr, _: jobject, self: jlong, a1: A, a2: B, ..., an: N): Ret {
     *     *WrapJavaEntity*(Java_CFFI_getFromRegistry<I>(env, self).foo(a1, a2, ..., an))
     * }
     */

    void PushEnvParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "env");

    void PushObjParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "obj");

    void PushSelfParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "self");

    bool FillMethodParamsByArg(std::vector<OwnedPtr<FuncParam>>& params, std::vector<OwnedPtr<FuncArg>>& callArgs,
        FuncDecl& funcDecl, OwnedPtr<FuncParam>& arg, FuncParam& jniEnvPtrParam);

    OwnedPtr<Decl> GenerateNativeMethod(FuncDecl& sampleMethod, Decl& decl);

    void GenerateFuncParamsForNativeDeleteCjObject(
        Decl& decl, std::vector<OwnedPtr<FuncParam>>& params, FuncParam*& jniEnv, OwnedPtr<Expr>& selfRef);

    OwnedPtr<Decl> GenerateNativeFuncDeclBylambda(Decl& decl, OwnedPtr<LambdaExpr>& wrappedNodesLambda,
        std::vector<OwnedPtr<FuncParamList>>& paramLists, FuncParam& jniEnvPtrParam, Ptr<Ty>& retTy,
        std::string funcName);

    std::string GetJniMethodName(const FuncDecl& method);

    std::string GetJniMethodNameForProp(const PropDecl& propDecl, bool isSet) const;

    std::string GetJniSuperArgFuncName(const ClassLikeDecl& outer, const std::string& id) const;

    std::string GetJniInitCjObjectFuncName(const FuncDecl& ctor, bool isGeneratedCtor);

    std::string GetJniInitCjObjectFuncNameForVarDecl(const VarDecl& ctor) const;

    std::string GetJniDeleteCjObjectFuncName(const Decl& decl) const;

    /**
     * @C public func Java_<type_signature>_deleteCJObject(env: JNIEnv_ptr, obj: jobject, self: jlong): Unit {
     *     Java_CFFI_deleteCJObject<type>(env, self, { objToDelete: <type> => objToDelete.javaref })
     * }
     */
    OwnedPtr<Decl> GenerateNativeDeleteCjObjectFunc(ClassLikeDecl& javaImpl, VarDecl& javaWeakRefField);

    /**
     * @C public func Java_Vector_deleteCJObject(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
     *      withExceptionHandling(env) {
     *          Java_CFFI_removeFromRegistry(self)
     *     }
     *   }
     */
    OwnedPtr<Decl> GenerateCJMappingNativeDeleteCjObjectFunc(Decl& decl);

    // A helper function to for the ctor of Enum.
    void GenerateNativeInitCJObjectEnumCtor(AST::EnumDecl& enumDecl);

    /**
     * @C public func Java_<enum_identifier>_initCJObject(env: JNIEnv_ptr, obj: jobject): jlong {
           return withExceptionHandling(env, { =>
               return Java_CFFI_put_to_registry_1(Enum.VarDecl)
           })
     * }
     */
    OwnedPtr<Decl> GenerateNativeInitCjObjectFuncForEnumCtorNoParams(AST::EnumDecl& enumDecl, AST::VarDecl& ctor);

    /**
     * when arg ClassLikeDecl is true(e.g. calss decl)::
     *
     * @C public func Java_<type_signature>_initCJObject{mangled}(env: JNIEnv_ptr, obj: jobject, args...): jlong {
     *     registry.put(
     *         <type>(Java_CFFI_newGlobalReference(env, Java_CFFI_JavaEntity(JOBJECT, obj), true), args...)
     *     )
     * }
     * when arg ClassLikeDecl is false (e.g. struct decl):
     *
     * @C public func Java_<type_signature>_initCJObject{mangled}(env: JNIEnv_ptr, obj: jobject, args...): jlong {
     *     registry.put(
     *         <type>(args...)
     *     )
     * }
     */
    OwnedPtr<Decl> GenerateNativeInitCjObjectFunc(FuncDecl& ctor, bool isClassLikeDecl);

    /**
     * for func [fun]:
     *     func foo(args): Ret
     *
     * the following will be generated:
     *     func foo(args): Ret {
     *         *UnwrapJavaEntity*(
     *             Java_CFFI_callMethod_raw(
     *                 Java_CFFI_get_env(),
     *                 this.javaref, // or getJavaref if mirror is an interface
     *                 typeSignature, "foo", "(<argsSignature>)Ret",
     *                 [Java_CFFI_JavaEntity(args[0]), ... Java_CFFI_JavaEntity(args[n])]
     *         )
     *     }
     *
     * where *UnwrapJavaEntity* - generated unwrapper for Ret type value.
     */
    void DesugarJavaMirrorMethod(FuncDecl& fun, ClassLikeDecl& mirror);

    /**
     * used in DesugarJavaMirrorMethod for method's body generation
     *
     */
    void AddJavaMirrorMethodBody(const ClassLikeDecl& mirror, FuncDecl& fun, OwnedPtr<Expr> javaRefCall);

    /**
     * for prop [prop]:
     *   mut prop p: Ret
     *
     * the following will be generated:
     *     mut prop p: Ret {
     *         get() {
     *             *UnwrapJavaEntity*(
     *                 Java_CFFI_getField_raw(
     *                     Java_CFFI_get_env(), this.javaref, typeSignature, "p", "Ret"
     *             ))
     *         }
     *         set(v) {
     *             Java_CFFI_setField_raw(
     *                 Java_CFFI_get_env(),
     *                 this.javaref, typeSignature, "p", "Ret", Java_CFFI_JavaEntity(v)
     *             )
     *         }
     *     }
     */
    void DesugarJavaMirrorProp(PropDecl& prop);

    void InsertJavaMirrorPropGetter(PropDecl& prop);
    void InsertJavaMirrorPropSetter(PropDecl& prop);

    /**
     * For CType ty, ty is returned. For mirrors, impls and CJMapping jobject is returned
     */
    Ptr<Ty> GetJNITy(Ptr<Ty> ty);

    OwnedPtr<Expr> CreateIsInstanceCall(Ptr<VarDecl> jObjectVar, Ptr<Ty> classTy, Ptr<File> curFile);

    /**
     * Transforms `x is T` expressions, where T is Java class into and x is class or interface:
     *
     * match (x) {
     *   case x : JObject =>
     *     Java_CFFI_isInstanceOf(Java_CFFI_get_env(), x.javaref, [Name of T Java class])
     *   case _ => false
     * }
     */
    void DesugarIsExpression(IsExpr& ie);

    OwnedPtr<Expr> CreateJObjectCast(Ptr<VarDecl> jObjectVar, Ptr<ClassLikeDecl> castDecl, Ptr<File> curFile);

    /**
     * Transforms `x as T` expressions, similarly to `is` above:
     *
     * In case of T is JavaMirror:
     * match (x) {
     *   case x : JObject =>
     *     match (Java_CFFI_isInstanceOf(Java_CFFI_get_env(), x.javaref, [Name of T Java class])) {
     *       case true =>
     *         (When T is JavaMirror) Some(T(x.javaref))
     *         (When T is JavaImpl) Java_CFFI_getFromRegistryByObj<T>(Java_CFFI_get_env(), x.javaref)
     *       case flase => None
     *     }
     *   case _ => None
     * }
     * }
     */
    void DesugarAsExpression(AsExpr& ae);

    /**
     * Transforms case arm where Java class is used in type patterns
     *
     * `case (.. (xi : Ti) ..) where guard => ...`
     * where Ti is Java class. Gets replaced with:
     * case (.. (xi : JObject) ..) where IsInstanceOf(xi.javaref, Ti) && .. && guard => {
     *  (.. let xi$Casted = T(x.javaref) ..)
     *  // All references to xi are replaces with references to xi$Casted
     *  ...
     * }
     *
     * Guard also might use xi variables, so casted variables are added to guard too and
     * all references are replaced
     */
    void DesugarMatchCase(MatchCase& matchCase);
    OwnedPtr<Block> CastAndSubstituteVars(
        Expr& expr, const std::vector<std::tuple<Ptr<VarDecl>, Ptr<Ty>>>& patternVars);

    /**
     * Transforms let pattern (let Some(a) = b), where type pattern with Java class is used
     *
     * let (... (xi : Ti) ...) = ...
     * where Ti is Java class. Gets replaced with:
     * let ( ... (xi : JObject) ... ) = ... && IsInstanceOf(xi, Ti) && ...
     */
    void DesugarLetPattern(LetPatternDestructor& letPat);

    /**
     * Inserts constructor of form `JString(String)`.
     * The operation consists of two steps:
     * 1) Insert constructor stub (constructor with empty body): [doStub] = `true`
     * 2) Fills generated constructor with actual body: [doStub] = `false`
     *
     * public init(s: String) {
     *   super(Java_CFFI_CangjieStringToJava(env, s))
     * }
     */
    void InsertJStringOfStringCtor(ClassDecl& decl, bool doStub);

    void ReplaceCallsWithArrayJavaEntityGet(File& file);
    void ReplaceCallsWithArrayJavaEntitySet(File& file);

    void DesugarSuperMethodCall(CallExpr& call, ClassDecl& impl);

    void GenerateInJavaImpl(AST::ClassDecl* classDecl);
    void GenerateForCJStructMapping(AST::StructDecl* structDecl);
    void GenerateForCJEnumMapping(AST::EnumDecl& enumDecl);
    OwnedPtr<PrimitiveType> CreateUnitType();

    ImportManager& importManager;
    TypeManager& typeManager;
    Utils utils;
    DiagnosticEngine& diag;
    const BaseMangler& mangler;
    InteropLibBridge lib;
    const std::optional<std::string>& javaCodeGenPath;
    const std::string& outputLibPath;

    /**
     * Top-level declarations generated during desugaring. Should be added at the end of file desugaring
     */
    std::vector<OwnedPtr<Decl>> generatedDecls;
};

} // namespace Cangjie::Interop::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER
