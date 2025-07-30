// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the Desugar functions used after instantiation step.
 */

#include "DesugarInTypeCheck.h"

#include <atomic>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "AutoBoxing.h"
#include "ExtendBoxMarker.h"
#include "TypeCheckUtil.h"
#include "TypeCheckerImpl.h"

#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie;
using namespace AST;
using namespace Meta;
using namespace TypeCheckUtil;

// Perform desugar after generic instantiation.
void TypeChecker::PerformDesugarAfterInstantiation(ASTContext& ctx, Package& pkg) const
{
    impl->PerformDesugarAfterInstantiation(ctx, pkg);
}

namespace {
inline void UpdateDeclAttributes(Package& pkg, bool exportForTest)
{
    Walker(&pkg, [&exportForTest](auto node) {
        if (auto vd = DynamicCast<VarDecl*>(node); vd && vd->initializer) {
            vd->EnableAttr(Attribute::DEFAULT);
        }
        if (exportForTest) {
            if (auto fd = DynamicCast<FuncDecl*>(node); fd && !fd->TestAttr(Attribute::PRIVATE)) {
                auto isExtend = Is<ExtendDecl>(fd->outerDecl);
                auto isForeignFunc = fd->TestAttr(Attribute::FOREIGN);
                if (!isExtend && !isForeignFunc) {
                    return VisitAction::WALK_CHILDREN;
                }
                // Skip declarations added by the compiler because they wouldn't be accessible in tests anyway
                if (isExtend && fd->outerDecl->TestAttr(Attribute::COMPILER_ADD)) {
                    return VisitAction::WALK_CHILDREN;
                }
                fd->linkage = Linkage::EXTERNAL;
                if (fd->propDecl) {
                    fd->propDecl->linkage = Linkage::EXTERNAL;
                }
                if (isExtend) {
                    fd->outerDecl->EnableAttr(Attribute::FOR_TEST);
                } else {
                    fd->EnableAttr(Attribute::FOR_TEST);
                }
            }
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
}
} // namespace

void TypeChecker::TypeCheckerImpl::PerformDesugarAfterInstantiation(ASTContext& ctx, Package& pkg)
{
    if (pkg.files.empty()) {
        return;
    }
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    if (!ci->invocation.globalOptions.disableReflection) {
        PerformToAnyInsertion(pkg);
    }
#endif
    if (!ci->invocation.globalOptions.disableInstantiation) {
        AutoBoxing autoBox(typeManager, importManager, ctx);
        autoBox.AddExtendBox(pkg);
    }
    // For bep: decls need to sort by identifier to make sequence stable.
    auto compare = [](Ptr<const ClassDecl> cd1, Ptr<const ClassDecl> cd2) {
        return cd1->identifier.Val() > cd2->identifier.Val();
    };
    std::set<Ptr<ClassDecl>, decltype(compare)> autoBoxedDeclBases(compare);
    std::for_each(ctx.typeToAutoBoxedDeclBaseMap.begin(), ctx.typeToAutoBoxedDeclBaseMap.end(),
        [&autoBoxedDeclBases](auto& it) { autoBoxedDeclBases.insert(it.second.release()); });
    for (auto i : autoBoxedDeclBases) {
        i->curFile = pkg.files[0].get();
        i->fullPackageName = pkg.fullPackageName;
        for (auto& decl : i->body->decls) {
            decl->curFile = i->curFile;
            decl->fullPackageName = pkg.fullPackageName;
        }
        pkg.files[0]->decls.push_back(OwnedPtr<ClassDecl>(i));
    }
    std::set<Ptr<ClassDecl>, decltype(compare)> autoBoxedDecls(compare);
    std::for_each(ctx.typeToAutoBoxedDeclMap.begin(), ctx.typeToAutoBoxedDeclMap.end(),
        [&autoBoxedDecls](auto& it) { autoBoxedDecls.insert(it.second.release()); });
    for (auto i : autoBoxedDecls) {
        i->curFile = pkg.files[0].get();
        i->fullPackageName = pkg.fullPackageName;
        for (auto& decl : i->body->decls) {
            decl->curFile = i->curFile;
            decl->fullPackageName = pkg.fullPackageName;
        }
        pkg.files[0]->decls.push_back(OwnedPtr<ClassDecl>(i));
    }
    ctx.typeToAutoBoxedDeclBaseMap.clear();
    ctx.typeToAutoBoxedDeclMap.clear();
    PerformRecursiveTypesElimination();
    UpdateDeclAttributes(pkg, ci->invocation.globalOptions.exportForTest);
}

namespace {
/**
 * If any varPattern in mc->pattern is upcast or downcast,
 * we need to change the refExpr of varPattern to the box or unbox expr.
 * e.g  x need to box, and ref x need change to boxed expr $Box_T(x)
 * case x: I => x.f()
 */
VisitAction RearrangePreVisit(Ptr<Node> node)
{
    if (auto re = DynamicCast<AST::RefExpr*>(node);
        re && re->desugarExpr && re->desugarExpr->astKind == ASTKind::MEMBER_ACCESS) {
        // Avoid re-enter desugar process. RefExpr is desugar as MemberAccess only in unbox.
        return VisitAction::SKIP_CHILDREN;
    }
    return VisitAction::WALK_CHILDREN;
}
VisitAction RearrangePostVisit(Ptr<Node> node)
{
    if (node->astKind == ASTKind::REF_EXPR) {
        auto re = StaticAs<ASTKind::REF_EXPR>(node);
        auto vd = DynamicCast<AST::VarDecl*>(re->ref.target);
        if (vd == nullptr) {
            return VisitAction::WALK_CHILDREN;
        }
        auto vp = DynamicCast<AST::VarPattern*>(vd->parentPattern);
        if (vp != nullptr && vp->desugarExpr != nullptr && re->desugarExpr == nullptr) {
            re->desugarExpr = ASTCloner::Clone(vp->desugarExpr.get());
            re->desugarExpr->mapExpr = vp->desugarExpr.get();
        }
        return VisitAction::WALK_CHILDREN;
    }
    return VisitAction::WALK_CHILDREN;
}
void RearrangeRefExpr(const MatchCase& mc)
{
    Walker walker(mc.exprOrDecls.get(), RearrangePreVisit, RearrangePostVisit);
    walker.Walk();
    Walker walker2(mc.patternGuard.get(), RearrangePreVisit, RearrangePostVisit);
    walker2.Walk();
}

void RearrangeRefExpr(Expr& e)
{
    Walker walker(&e, RearrangePreVisit, RearrangePostVisit);
    walker.Walk();
}
} // namespace

void AutoBoxing::AddExtendBox(Package& pkg)
{
    // Mark boxing point first.
    {
        std::lock_guard lockGuard{ExtendBoxMarker::mtx};
        Walker(&pkg, ExtendBoxMarker::GetMarkExtendBoxFunc(typeManager)).Walk();
    }
    // Define boxing process.
    std::function<VisitAction(Ptr<Node>)> postVisit = [this](Ptr<Node> node) -> VisitAction {
        if (node->TestAttr(Attribute::GENERIC)) {
            return VisitAction::SKIP_CHILDREN;
        }
        return match(*node)([this](VarDecl& vd) { return AutoBoxVarDecl(vd); },
            [this](AssignExpr& ae) { return AutoBoxAssignExpr(ae); },
            [this](CallExpr& ce) { return AutoBoxCallExpr(ce); }, [this](IfExpr& ie) { return AutoBoxIfExpr(ie); },
            [this](WhileExpr& we) { return AutoBoxWhileExpr(we); },
            [this](ReturnExpr& re) { return AutoBoxReturnExpr(re); },
            [this](ArrayLit& lit) { return AutoBoxArrayLit(lit); },
            [this](MatchExpr& me) { return AutoBoxMatchExpr(me); }, [this](TryExpr& te) { return AutoBoxTryExpr(te); },
            [this](TupleLit& tl) { return AutoBoxTupleLit(tl); },
            [this](ArrayExpr& ae) { return AutoBoxArrayExpr(ae); }, []() { return VisitAction::WALK_CHILDREN; });
    };
    std::function<VisitAction(Ptr<Node>)> preVist = [&pkg](auto node) -> VisitAction {
        // Skip children for unchanged incremental compilation.
        if (pkg.TestAttr(Attribute::INCRE_COMPILE)) {
            // Skip unchanged global/member function or variables.
            if (auto decl = DynamicCast<Decl*>(node); decl && IsGlobalOrMember(*decl) && !decl->toBeCompiled &&
                (decl->astKind == ASTKind::FUNC_DECL || decl->astKind == ASTKind::VAR_DECL)) {
                return VisitAction::SKIP_CHILDREN;
            }
        }
        return node->TestAttr(Attribute::GENERIC) ? VisitAction::SKIP_CHILDREN : VisitAction::WALK_CHILDREN;
    };
    // Since node type may be used to check with child ty for boxing, we need boxing child first.
    Walker walker(&pkg, preVist, postVisit);
    walker.Walk();
}

VisitAction AutoBoxing::AutoBoxReturnExpr(ReturnExpr& re)
{
    if (re.TestAttr(Attribute::NEED_AUTO_BOX)) {
        auto& expr = re.expr;
        auto funcTy = StaticCast<FuncTy*>(re.refFuncBody->ty);
        CJC_NULLPTR_CHECK(expr->curFile);
        // If the expr has attribute, it is used inside compiler added 'toAny' function, and only need base box.
        Ptr<ClassDecl> autoBoxedType = expr->TestAttr(Attribute::NO_REFLECT_INFO)
            ? GetBoxedBaseDecl(*expr->curFile, *expr->ty, *funcTy->retTy)
            : GetBoxedDecl(*expr->curFile, *expr->ty, *funcTy->retTy);
        re.expr = AutoBoxingCallExpr(std::move(expr), *autoBoxedType);
        re.DisableAttr(Attribute::NEED_AUTO_BOX);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxVarDecl(VarDecl& vd)
{
    if (vd.TestAttr(Attribute::NEED_AUTO_BOX)) {
        auto& expr = vd.initializer;
        CJC_NULLPTR_CHECK(expr->curFile);
        Ptr<ClassDecl> autoBoxedBaseType = GetBoxedDecl(*expr->curFile, *vd.initializer->ty, *vd.ty);
        vd.initializer = AutoBoxingCallExpr(std::move(expr), *autoBoxedBaseType);
        vd.DisableAttr(Attribute::NEED_AUTO_BOX);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxAssignExpr(AssignExpr& ae)
{
    if (ae.TestAttr(Attribute::NEED_AUTO_BOX)) {
        auto& expr = ae.rightExpr;
        CJC_NULLPTR_CHECK(expr->curFile);
        Ptr<ClassDecl> autoBoxedBaseType = GetBoxedDecl(*expr->curFile, *ae.rightExpr->ty, *ae.leftValue->ty);
        ae.rightExpr = AutoBoxingCallExpr(std::move(expr), *autoBoxedBaseType);
        ae.DisableAttr(Attribute::NEED_AUTO_BOX);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxArrayExpr(ArrayExpr& ae)
{
    if (!ae.TestAttr(Attribute::NEED_AUTO_BOX) || ae.args.empty()) {
        return VisitAction::WALK_CHILDREN;
    }
    auto targetTy = typeManager.GetTypeArgs(*ae.ty)[0];
    if (ae.initFunc) {
        auto initFuncTy = DynamicCast<FuncTy*>(ae.initFunc->ty);
        if (initFuncTy == nullptr) {
            return VisitAction::WALK_CHILDREN;
        }
        targetTy = initFuncTy->paramTys[1];
    }

    // For RawArray(collection) boxing argIndex is 0.
    // For RawArray(size, item:T) boxing argIndex is 1.
    // For VArray<...>(repeat:T) boxing argIndex is 0.
    auto& arg = ae.initFunc || ae.isValueArray ? ae.args[0] : ae.args[1];
    auto& argExpr = arg->expr;
    CJC_NULLPTR_CHECK(argExpr->curFile);
    Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*argExpr->curFile, *argExpr->ty, *targetTy);
    arg->expr = AutoBoxingCallExpr(std::move(argExpr), *autoBoxedType);
    arg->ty = arg->expr->ty;
    ae.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxTupleLit(TupleLit& tl)
{
    if (!tl.TestAttr(Attribute::NEED_AUTO_BOX)) {
        return VisitAction::WALK_CHILDREN;
    }
    auto tupleTy = StaticCast<TupleTy*>(tl.ty);
    auto typeArgs = tupleTy->typeArgs;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (tl.children[i]->ty && typeArgs[i] && typeManager.HasExtensionRelation(*tl.children[i]->ty, *typeArgs[i])) {
            auto& expr = tl.children[i];
            CJC_NULLPTR_CHECK(expr->curFile);
            Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*expr->curFile, *tl.children[i]->ty, *typeArgs[i]);
            tl.children[i] = AutoBoxingCallExpr(std::move(expr), *autoBoxedType);
        }
    }
    tl.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

bool AutoBoxing::AutoBoxOrUnboxTypePatterns(TypePattern& typePattern, Ty& selectorTy)
{
    if (typePattern.pattern == nullptr || typePattern.desugarVarPattern) {
        return false;
    }
    CJC_ASSERT(typePattern.ty && typePattern.type && typePattern.ty == typePattern.type->ty);
    bool isDowncast = typeManager.HasExtensionRelation(*typePattern.ty, selectorTy) ||
        ExtendBoxMarker::MustUnboxDownCast(selectorTy, *typePattern.ty);
    if (isDowncast) {
        // Downcast.
        UnBoxingTypePattern(typePattern, selectorTy);
        return true;
    } else if (typeManager.HasExtensionRelation(selectorTy, *typePattern.type->ty)) {
        // Upcast.
        AutoBoxTypePattern(typePattern, selectorTy);
        return true;
    }
    return false;
}

bool AutoBoxing::AutoBoxOrUnboxPatterns(Pattern& pattern, Ty& selectorTy)
{
    bool boxOrUnbox = false;
    switch (pattern.astKind) {
        case ASTKind::TYPE_PATTERN: {
            if (AutoBoxOrUnboxTypePatterns(*StaticAs<ASTKind::TYPE_PATTERN>(&pattern), selectorTy)) {
                boxOrUnbox = true;
            }
            break;
        }
        case ASTKind::TUPLE_PATTERN: {
            auto tuplePattern = RawStaticCast<AST::TuplePattern*>(&pattern);
            auto tupleTy = DynamicCast<AST::TupleTy*>(&selectorTy);
            for (size_t i = 0; i < tuplePattern->patterns.size(); i++) {
                CJC_ASSERT(tuplePattern && tuplePattern->patterns[i]);
                CJC_ASSERT(tupleTy && tupleTy->typeArgs[i]);
                if (AutoBoxOrUnboxPatterns(*tuplePattern->patterns[i], *tupleTy->typeArgs[i])) {
                    boxOrUnbox = true;
                }
            }
            break;
        }
        case ASTKind::ENUM_PATTERN: {
            auto enumPattern = RawStaticCast<EnumPattern*>(&pattern);
            auto constructorTy = DynamicCast<AST::FuncTy*>(enumPattern->constructor->ty);
            if (constructorTy == nullptr) {
                return boxOrUnbox;
            }
            CJC_ASSERT(constructorTy->paramTys.size() == enumPattern->patterns.size());
            for (size_t i = 0; i < enumPattern->patterns.size(); i++) {
                auto paramTy = constructorTy->paramTys[i];
                CJC_ASSERT(paramTy);
                CJC_NULLPTR_CHECK(enumPattern->patterns[i]);
                if (AutoBoxOrUnboxPatterns(*enumPattern->patterns[i], *paramTy)) {
                    boxOrUnbox = true;
                }
            }
            break;
        }
        default:
            break;
    }
    return boxOrUnbox;
}

/**
 *  1. If type pattern is downcast, the type of the type pattern need change to the boxed
 *     type. Create a new VarPattern node $tmpN: $Box_T saved in typePattern.desugarVarPattern
 *    e.g. (10, c: B)  Create new VarPattern nodes $tmp1: $Box_B saved in typePattern.desugarVarPattern
 *  2. Create the MemberAccess nodes $tmpN.$value  for varDecls in the type patterns which were mentioned above.
 *    e.g. (10, c: B)  will create a MemberAccess node: $tmp1.$value
 * */
void AutoBoxing::UnBoxingTypePattern(TypePattern& typePattern, const Ty& selectorTy)
{
    auto ty = typePattern.type->ty;
    CJC_ASSERT(typePattern.curFile);
    Ptr<ClassDecl> autoBoxedBaseType = GetBoxedBaseDecl(*typePattern.curFile, *ty, selectorTy);
    CJC_ASSERT(autoBoxedBaseType);

    auto boxType = CreateRefType(*autoBoxedBaseType);
    auto varDecl = CreateTmpVarDecl();
    varDecl->ty = autoBoxedBaseType->ty;
    varDecl->type = std::move(boxType);

    auto refExpr = MakeOwned<RefExpr>();
    refExpr->ref.identifier = varDecl->identifier;
    refExpr->ref.target = varDecl.get();
    refExpr->ty = autoBoxedBaseType->ty;
    refExpr->EnableAttr(Attribute::COMPILER_ADD);

    auto ma = MakeOwned<MemberAccess>();
    ma->field = "$value";
    ma->baseExpr = std::move(refExpr);
    ma->EnableAttr(Attribute::COMPILER_ADD);

    for (auto& decl : autoBoxedBaseType->body->decls) {
        if (auto vd = DynamicCast<VarDecl*>(decl.get()); vd) {
            ma->target = vd;
            ma->ty = vd->ty;
        }
    }

    // Set the unboxed member-access expr in the original type pattern's var pattern.
    // NOTE: if the sub-pattern is wildcard pattern, discard the memberAccess expr.
    auto vp = DynamicCast<AST::VarPattern*>(typePattern.pattern.get());
    if (vp != nullptr) {
        vp->desugarExpr = std::move(ma);
    }

    auto varPattern = MakeOwned<VarPattern>();
    varPattern->varDecl = std::move(varDecl);
    varPattern->ty = autoBoxedBaseType->ty;
    varPattern->varDecl->parentPattern = varPattern.get();
    varPattern->EnableAttr(Attribute::COMPILER_ADD);

    typePattern.desugarVarPattern = std::move(varPattern);

    // source is changed
    typePattern.needRuntimeTypeCheck = true;
    typePattern.matchBeforeRuntime = false;
}

/**
 *  If type pattern is upcast from T to I
 *  1. Create a new VarPattern node $tmpN: T, saved in typePattern.desugarVarPattern
 *  2. Create the CallExpr nodes $Box_T($tmpN), saved in typePattern.desugarExpr for chir,
 *     and saved in typePattern.pattern->desugarExpr for reArrangeRefExpr.
 * */
void AutoBoxing::AutoBoxTypePattern(TypePattern& typePattern, Ty& selectorTy)
{
    if (typePattern.pattern->astKind != ASTKind::VAR_PATTERN) {
        return;
    }

    auto refExpr = MakeOwned<RefExpr>();
    CJC_ASSERT(typePattern.curFile);
    Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*typePattern.curFile, selectorTy, *typePattern.ty);

    auto varPattern = RawStaticCast<VarPattern*>(typePattern.pattern.get());
    CJC_ASSERT(varPattern->varDecl);

    auto varDecl = CreateTmpVarDecl();
    varDecl->ty = &selectorTy;
    refExpr = CreateRefExpr(*varDecl);

    auto desugarVarPattern = MakeOwned<VarPattern>();
    desugarVarPattern->varDecl = std::move(varDecl);
    desugarVarPattern->ty = &selectorTy;
    desugarVarPattern->varDecl->parentPattern = desugarVarPattern.get();
    desugarVarPattern->EnableAttr(Attribute::COMPILER_ADD);
    auto tmpRefExpr = ASTCloner::Clone(refExpr.get());

    typePattern.desugarExpr = AutoBoxingCallExpr(std::move(refExpr), *autoBoxedType);
    typePattern.desugarVarPattern = std::move(desugarVarPattern);
    varPattern->desugarExpr = AutoBoxingCallExpr(std::move(tmpRefExpr), *autoBoxedType);
    // Upcast means this case is matched.
    typePattern.matchBeforeRuntime = true;
    typePattern.needRuntimeTypeCheck = false;
}

VisitAction AutoBoxing::AutoBoxMatchExpr(MatchExpr& me)
{
    if (!me.TestAttr(Attribute::NEED_AUTO_BOX)) {
        return VisitAction::WALK_CHILDREN;
    }
    if (!me.matchMode) {
        // If no selector exists in match, only box cases.
        for (auto& mc : me.matchCaseOthers) {
            CJC_ASSERT(mc->exprOrDecls);
            AutoBoxBlock(*mc->exprOrDecls, *me.ty);
        }
        me.DisableAttr(Attribute::NEED_AUTO_BOX);
        return VisitAction::WALK_CHILDREN;
    }

    CJC_ASSERT(me.selector);
    CJC_ASSERT(me.selector->ty);

    for (auto& mc : me.matchCases) {
        /* Handle the case where the value of match-case body requires box. */
        CJC_ASSERT(mc->exprOrDecls);
        AutoBoxBlock(*mc->exprOrDecls, *me.ty);

        // match expression without selector
        if (!me.matchMode) {
            me.DisableAttr(Attribute::NEED_AUTO_BOX);
            return VisitAction::WALK_CHILDREN;
        }

        /* unbox downcast type pattern and box upcast type pattern, if the mc exits unbox create desugarCase. */
        CJC_ASSERT(!mc->patterns.empty());
        for (auto& pattern : mc->patterns) {
            CJC_NULLPTR_CHECK(pattern);
            if (AutoBoxOrUnboxPatterns(*pattern, *me.selector->ty)) {
                RearrangeRefExpr(*mc);
            }
        }
    }

    me.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

void AutoBoxing::AutoBoxBlock(Block& block, Ty& ty)
{
    // If the block is empty or end with declaration, the last type is 'Unit',
    // otherwise the last type is the type of last expression.
    auto lastExprOrDecl = block.GetLastExprOrDecl();
    Ptr<Ty> lastTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    if (auto expr = DynamicCast<Expr*>(lastExprOrDecl.get()); expr) {
        lastTy = expr->ty;
    }
    if (!lastTy || !typeManager.HasExtensionRelation(*lastTy, ty)) {
        return;
    }
    // If the block is empty or end with declaration, we need to insert a unitExpr for box.
    if (Is<Decl>(lastExprOrDecl) || block.body.empty()) {
        auto unitExpr = CreateUnitExpr(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
        unitExpr->curFile = block.curFile;
        lastExprOrDecl = unitExpr.get();
        block.body.emplace_back(std::move(unitExpr));
    }
    auto lastExprOrDeclOwned = std::move(block.body.back());
    block.body.pop_back();

    CJC_ASSERT(Is<Expr>(lastExprOrDecl));
    OwnedPtr<Expr> ownedExpr(StaticCast<Expr*>(lastExprOrDeclOwned.release()));
    CJC_NULLPTR_CHECK(ownedExpr->curFile);
    Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*ownedExpr->curFile, *ownedExpr->ty, ty);
    block.body.emplace_back(AutoBoxingCallExpr(std::move(ownedExpr), *autoBoxedType));
    block.ty = block.body.back()->ty;
}

VisitAction AutoBoxing::AutoBoxTryExpr(TryExpr& te)
{
    if (!te.TestAttr(Attribute::NEED_AUTO_BOX)) {
        return VisitAction::WALK_CHILDREN;
    }
    if (te.tryBlock) {
        AutoBoxBlock(*te.tryBlock, *te.ty);
    }
    for (auto& ce : te.catchBlocks) {
        AutoBoxBlock(*ce, *te.ty);
    }
    te.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxArrayLit(ArrayLit& lit)
{
    if (lit.TestAttr(Attribute::NEED_AUTO_BOX)) {
        for (auto& c : lit.children) {
            if (c->ty && lit.ty->typeArgs[0] && typeManager.HasExtensionRelation(*c->ty, *lit.ty->typeArgs[0])) {
                CJC_NULLPTR_CHECK(c->curFile);
                Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*c->curFile, *c->ty, *lit.ty->typeArgs[0]);
                c = AutoBoxingCallExpr(std::move(c), *autoBoxedType);
            }
        }
        lit.DisableAttr(Attribute::NEED_AUTO_BOX);
        lit.DisableAttr(Attribute::NEED_AUTO_BOX);
    }
    return VisitAction::WALK_CHILDREN;
}

bool AutoBoxing::AutoBoxCondition(Expr& condition)
{
    if (auto let = DynamicCast<LetPatternDestructor>(&condition)) {
        bool res{false};
        for (auto& pat : std::as_const(let->patterns)) {
            if (pat->TestAttr(Attribute::NEED_AUTO_BOX)) {
                if (AutoBoxOrUnboxPatterns(*pat, *let->initializer->ty)) {
                    res = true;
                }
                pat->DisableAttr(Attribute::NEED_AUTO_BOX);
            }
        }
        return res;
    }
    if (auto par = DynamicCast<ParenExpr>(&condition)) {
        return AutoBoxCondition(*par->expr);
    }
    if (auto bin = DynamicCast<BinaryExpr>(&condition);
        bin && (bin->op == TokenKind::AND || bin->op == TokenKind::OR)) {
        bool res{false};
        if (AutoBoxCondition(*bin->leftExpr)) {
            res = true;
            RearrangeRefExpr(*bin->rightExpr);
        }
        return AutoBoxCondition(*bin->rightExpr) || res;
    }
    condition.DisableAttr(Attribute::NEED_AUTO_BOX);
    return false;
}

VisitAction AutoBoxing::AutoBoxIfExpr(IfExpr& ie)
{
    // the outermost condition is always marked if there are condition that requires box in this condition
    if (ie.condExpr->TestAttr(Attribute::NEED_AUTO_BOX)) {
        if (AutoBoxCondition(*ie.condExpr)) {
            RearrangeRefExpr(*ie.thenBody);
        }
    }
    if (!ie.TestAttr(Attribute::NEED_AUTO_BOX)) {
        return VisitAction::WALK_CHILDREN;
    }
    if (ie.thenBody) {
        AutoBoxBlock(*ie.thenBody, *ie.ty);
    }
    if (ie.hasElse && ie.elseBody && Is<Block>(ie.elseBody.get())) {
        if (auto block = DynamicCast<Block*>(ie.elseBody.get()); block) {
            AutoBoxBlock(*block, *ie.ty);
        }
    }
    ie.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

AST::VisitAction AutoBoxing::AutoBoxWhileExpr(const AST::WhileExpr& we)
{
    if (we.condExpr->TestAttr(Attribute::NEED_AUTO_BOX)) {
        if (AutoBoxCondition(*we.condExpr)) {
            RearrangeRefExpr(*we.body);
        }
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AutoBoxCallExpr(CallExpr& ce)
{
    if (!ce.TestAttr(Attribute::NEED_AUTO_BOX)) {
        return VisitAction::WALK_CHILDREN;
    }
    unsigned count = 0;
    auto funcTy = StaticCast<FuncTy*>(ce.baseFunc->ty);
    if (ce.desugarArgs.has_value()) {
        for (auto& arg : *ce.desugarArgs) {
            if (count >= funcTy->paramTys.size()) {
                break;
            }
            auto paramTy = funcTy->paramTys[count];
            if (arg->expr && arg->expr->ty && paramTy && typeManager.HasExtensionRelation(*arg->expr->ty, *paramTy)) {
                auto& expr = arg->expr;
                CJC_NULLPTR_CHECK(expr->curFile);
                Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*expr->curFile, *arg->expr->ty, *paramTy);
                arg->expr = AutoBoxingCallExpr(std::move(expr), *autoBoxedType);
                arg->ty = arg->expr->ty;
            }
            count = count + 1;
        }
    } else {
        ce.desugarArgs = std::vector<Ptr<FuncArg>>();
        for (auto& arg : ce.args) {
            if (count >= funcTy->paramTys.size()) {
                break;
            }
            auto paramTy = funcTy->paramTys[count];
            if (arg->ty && paramTy && typeManager.HasExtensionRelation(*arg->ty, *paramTy)) {
                auto& expr = arg->expr;
                CJC_NULLPTR_CHECK(expr->curFile);
                Ptr<ClassDecl> autoBoxedType = GetBoxedDecl(*expr->curFile, *arg->ty, *paramTy);
                arg->expr = AutoBoxingCallExpr(std::move(expr), *autoBoxedType);
                arg->ty = arg->expr->ty;
            }
            ce.desugarArgs.value().push_back(arg.get());
            count = count + 1;
        }
    }
    ce.DisableAttr(Attribute::NEED_AUTO_BOX);
    return VisitAction::WALK_CHILDREN;
}

Ptr<ClassDecl> AutoBoxing::GetBoxedBaseDecl(File& curFile, Ty& ty, const Ty& iTy)
{
    Ptr<Ty> extendedTy = nullptr;
    extendedTy = typeManager.GetRealExtendedTy(ty, iTy);
    // Find Box decls in current package.
    if (ctx.typeToAutoBoxedDeclBaseMap.find(extendedTy) != ctx.typeToAutoBoxedDeclBaseMap.end()) {
        return ctx.typeToAutoBoxedDeclBaseMap.at(extendedTy).get();
    }

    // If no valid cache exist, create new boxed base ClassDecl in curren package.
    auto baseType = AutoBoxedBaseType(*extendedTy);
    AddCurFile(*baseType, &curFile);
    auto autoBoxedBaseType = baseType.get();
    ctx.typeToAutoBoxedDeclBaseMap.emplace(std::pair{extendedTy, std::move(baseType)});
    return autoBoxedBaseType;
}

Ptr<ClassDecl> AutoBoxing::GetBoxedDecl(File& curFile, Ty& ty, const Ty& iTy)
{
    Ptr<ClassDecl> boxedDecl = nullptr;
    auto extendedTy = typeManager.GetRealExtendedTy(ty, iTy);
    if (ctx.typeToAutoBoxedDeclMap.find(extendedTy) != ctx.typeToAutoBoxedDeclMap.end()) {
        boxedDecl = ctx.typeToAutoBoxedDeclMap.at(extendedTy).get();
    } else {
        auto cd = AutoBoxedType(ctx.fullPackageName, *extendedTy, *GetBoxedBaseDecl(curFile, *extendedTy, iTy));
        boxedDecl = cd.get();
        ctx.typeToAutoBoxedDeclMap.emplace(std::pair{extendedTy, std::move(cd)});
    }
    return boxedDecl;
}

OwnedPtr<CallExpr> AutoBoxing::AutoBoxingCallExpr(OwnedPtr<AST::Expr>&& expr, const ClassDecl& cd) const
{
    auto ce = MakeOwned<CallExpr>();
    ce->callKind = AST::CallKind::CALL_OBJECT_CREATION;
    ce->ty = cd.ty;
    ce->EnableAttr(Attribute::COMPILER_ADD);

    for (auto& i : cd.body->decls) {
        if (auto fd = DynamicCast<FuncDecl*>(i.get()); fd && fd->TestAttr(Attribute::CONSTRUCTOR)) {
            ce->resolvedFunction = fd;
        }
    }

    auto baseFunc = CreateRefExpr(cd.identifier);
    baseFunc->ref.target = ce->resolvedFunction;
    baseFunc->callOrPattern = ce.get();
    baseFunc->ty = ce->resolvedFunction->ty;
    ce->baseFunc = std::move(baseFunc);

    auto exprTemp = expr.get();
    auto arg = CreateFuncArg(std::move(expr));
    ce->args.push_back(std::move(arg));
    CopyBasicInfo(exprTemp, ce.get());
    AddCurFile(*ce, exprTemp->curFile);
    return ce;
}

/**
 * Mangled base boxed type name must be unique and same in all packages.
 * So create name with related generic types.
 */
std::string AutoBoxing::GetAutoBoxedTypeName(const Ty& beBoxedType, bool isBaseBox)
{
    if (isBaseBox) { // it is a base box and need to be unique
        return MangleUtils::GetMangledNameOfCompilerAddedClass(BOX_DECL_PREFIX + mangler.MangleType(beBoxedType));
    }
    // other box type will be mangled normally
    return BOX_DECL_PREFIX + mangler.MangleType(beBoxedType);
}

void AutoBoxing::AddSuperClassForBoxedType(ClassDecl& cd, const Ty& beBoxedType)
{
    if (!beBoxedType.IsClass()) {
        return;
    }
    auto tyDecl = Ty::GetDeclOfTy<InheritableDecl>(&beBoxedType);
    CJC_ASSERT(tyDecl && tyDecl->astKind == ASTKind::CLASS_DECL);
    auto superClass = RawStaticCast<ClassDecl*>(tyDecl)->GetSuperClassDecl();
    if (superClass && Ty::IsTyCorrect(superClass->ty) && !ctx.curPackage->files.empty()) {
        // Add boxed super class as current class decl's super type.
        auto boxedSuper = GetBoxedBaseDecl(*ctx.curPackage->files[0], *superClass->ty, *typeManager.GetAnyTy());
        cd.inheritedTypes.insert(cd.inheritedTypes.begin(), CreateRefType(*boxedSuper));
    }
}

namespace {
OwnedPtr<CallExpr> AutoBoxedCreateSuperCall(ClassDecl& cd, Ptr<AST::VarDecl> vd = nullptr)
{
    auto superExpr = CreateRefExpr("super");
    superExpr->isSuper = true;
    superExpr->isAlone = false;
    auto superClass = cd.inheritedTypes.front().get();
    auto cTy = StaticCast<ClassTy*>(superClass->ty);
    for (auto& decl : cTy->decl->body->decls) {
        if (auto fd = DynamicCast<FuncDecl*>(decl.get()); fd && fd->TestAttr(Attribute::CONSTRUCTOR)) {
            superExpr->ref.target = fd;
            superExpr->ty = fd->ty;
        }
    }
    std::vector<OwnedPtr<FuncArg>> args;
    if (vd) {
        args.emplace_back(CreateFuncArg(CreateRefExpr(*vd), "", vd->ty));
    }
    auto superCall = CreateCallExpr(std::move(superExpr), std::move(args));
    superCall->callKind = CallKind::CALL_SUPER_FUNCTION;
    superCall->ty = superClass->ty;
    for (auto& decl : cTy->decl->body->decls) {
        if (auto fd = DynamicCast<FuncDecl*>(decl.get()); fd && fd->TestAttr(Attribute::CONSTRUCTOR)) {
            superCall->resolvedFunction = fd;
        }
    }
    return superCall;
}

OwnedPtr<FuncDecl> CreateCtorForBoxedBaseType(ClassDecl& cd, const std::string& pkgName, OwnedPtr<FuncBody> fb)
{
    OwnedPtr<FuncDecl> initFunc = MakeOwnedNode<FuncDecl>();
    initFunc->toBeCompiled = true; // For incremental compilation.
    initFunc->funcBody = std::move(fb);
    initFunc->funcBody->funcDecl = initFunc.get();
    initFunc->identifier = "init";
    initFunc->isConst = true;
    initFunc->ty = initFunc->funcBody->ty;
    initFunc->constructorCall = ConstructorCall::SUPER;
    initFunc->funcBody->parentClassLike = &cd;
    initFunc->outerDecl = &cd;
    initFunc->fullPackageName = pkgName;
    initFunc->EnableAttr(Attribute::CONSTRUCTOR, Attribute::IN_CLASSLIKE, Attribute::IMPLICIT_ADD, Attribute::INTERNAL);
    initFunc->linkage = Linkage::INTERNAL;
    return initFunc;
}

void AddObjectSuperClass(ImportManager& importManager, ClassDecl& cd)
{
    if (auto objectDecl = importManager.GetCoreDecl<InheritableDecl>(OBJECT_NAME)) {
        auto tmp = CreateRefType(*objectDecl);
        tmp->curFile = cd.curFile;
        cd.inheritedTypes.insert(cd.inheritedTypes.begin(), std::move(tmp));
    }
}
} // namespace

OwnedPtr<ClassDecl> AutoBoxing::AutoBoxedBaseType(Ty& beBoxedType)
{
    OwnedPtr<ClassDecl> cd = MakeOwnedNode<ClassDecl>();
    cd->identifier = GetAutoBoxedTypeName(beBoxedType);
    cd->doNotExport = true;
    cd->toBeCompiled = true; // For incremental compilation.
    cd->fullPackageName = ctx.fullPackageName;
    cd->linkage = Linkage::INTERNAL;
    cd->EnableAttr(Attribute::OPEN, Attribute::NO_MANGLE, Attribute::IMPLICIT_ADD, Attribute::NO_REFLECT_INFO,
        Attribute::INTERNAL);

    auto ty = MakeOwned<RefType>();
    ty->ty = &beBoxedType;

    // If the boxed type has super class, inherited boxed super class to support type checking with super class.
    AddSuperClassForBoxedType(*cd, beBoxedType);
    bool useObjectSuper = cd->inheritedTypes.empty();
    if (useObjectSuper) {
        AddObjectSuperClass(importManager, *cd);
    }
    // create class ty which is auto boxed type
    auto boxedTy = typeManager.GetClassTy(*cd, {});
    cd->ty = boxedTy;

    OwnedPtr<VarDecl> varDecl = CreateVarDecl("$value", nullptr, ty.get());
    varDecl->toBeCompiled = true; // For incremental compilation.
    varDecl->ty = &beBoxedType;
    varDecl->isConst = true;
    varDecl->outerDecl = cd.get();
    varDecl->fullPackageName = ctx.fullPackageName;
    varDecl->EnableAttr(Attribute::IN_CLASSLIKE);
    varDecl->EnableAttr(Attribute::PUBLIC);

    // Create an assignment which is "this.$value = $value"
    auto thisExpr = CreateRefExpr("this");
    thisExpr->isThis = true;
    thisExpr->ref.target = cd.get();
    thisExpr->ty = cd->ty;

    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto left = CreateMemberAccess(std::move(thisExpr), *varDecl);
    auto funcParam = CreateFuncParam("$value", std::move(ty));
    auto right = CreateRefExpr(*funcParam);
    auto assignment = CreateAssignExpr(std::move(left), std::move(right), unitTy);

    // Create super call `super()`
    auto superCall = AutoBoxedCreateSuperCall(*cd, useObjectSuper ? nullptr : funcParam.get());

    // create constructor
    OwnedPtr<FuncBody> funcBody = MakeOwnedNode<FuncBody>();
    funcBody->body = MakeOwnedNode<Block>();
    funcBody->body->ty = unitTy;
    funcBody->ty = typeManager.GetFunctionTy({&beBoxedType}, cd->ty);

    auto funcParamList = MakeOwnedNode<FuncParamList>();
    funcParamList->params.push_back(std::move(funcParam));
    funcBody->paramLists.push_back(std::move(funcParamList));
    funcBody->body->body.push_back(std::move(superCall));
    funcBody->body->body.push_back(std::move(assignment));
    auto re = CreateReturnExpr(CreateUnitExpr(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)));
    re->refFuncBody = funcBody.get();
    funcBody->body->body.push_back(std::move(re));

    cd->body = MakeOwnedNode<ClassBody>();
    cd->body->decls.push_back(std::move(varDecl));
    auto baseDecl = Ty::GetDeclPtrOfTy(&beBoxedType);
    // For cjnative backend:
    // The base boxed class will be regenerated in every package, so the mangled name of its ctor must alway be same.
    // Using the package name of the boxed decl's package if decl exists, otherwise using core package name.
    std::string funcPackageName = baseDecl ? baseDecl->fullPackageName : CORE_PACKAGE_NAME;
    cd->body->decls.push_back(CreateCtorForBoxedBaseType(*cd, funcPackageName, std::move(funcBody)));
    return cd;
}

OwnedPtr<ClassDecl> AutoBoxing::AutoBoxedType(const std::string& packageName, Ty& beBoxedType, ClassDecl& base)
{
    OwnedPtr<ClassDecl> cd = MakeOwnedNode<ClassDecl>();
    cd->EnableAttr(Attribute::COMPILER_ADD, Attribute::IMPLICIT_ADD, Attribute::NO_REFLECT_INFO, Attribute::INTERNAL);
    cd->identifier = GetAutoBoxedTypeName(beBoxedType, false);
    cd->fullPackageName = packageName;
    cd->linkage = Linkage::INTERNAL;
    cd->doNotExport = true;
    cd->toBeCompiled = true; // For incremental compilation.

    // create class ty which is auto boxed type
    auto boxedTy = typeManager.GetClassTy(*cd, {});
    cd->ty = boxedTy;

    // Base class decl
    auto superClassType = CreateRefType(base);
    cd->inheritedTypes.push_back(std::move(superClassType));
    cd->body = MakeOwnedNode<ClassBody>();

    Ptr<VarDecl> varDecl = nullptr;
    for (auto& i : base.body->decls) {
        if (i->identifier == "$value") {
            varDecl = StaticCast<VarDecl*>(i.get());
        }
    }
    auto type = MakeOwnedNode<RefType>();
    type->ty = &beBoxedType;
    auto funcParam = CreateFuncParam("$value", std::move(type));

    // Create super call `super($value)`
    auto superExpr = CreateRefExpr("super");
    superExpr->isSuper = true;
    for (auto& decl : base.body->decls) {
        if (auto fd = DynamicCast<FuncDecl*>(decl.get()); fd && fd->TestAttr(Attribute::CONSTRUCTOR)) {
            superExpr->ref.target = fd;
            superExpr->ty = fd->ty;
        }
    }

    auto argBase = CreateRefExpr("$value");
    argBase->ref.target = funcParam.get();
    argBase->ty = &beBoxedType;
    auto arg = MakeOwnedNode<FuncArg>();
    arg->expr = std::move(argBase);
    arg->ty = &beBoxedType;
    std::vector<OwnedPtr<FuncArg>> args;
    args.push_back(std::move(arg));
    auto superCall = CreateCallExpr(std::move(superExpr), std::move(args));
    superCall->callKind = AST::CallKind::CALL_SUPER_FUNCTION;
    superCall->ty = base.ty;
    for (auto& decl : base.body->decls) {
        if (auto fd = DynamicCast<FuncDecl*>(decl.get()); fd && fd->TestAttr(Attribute::CONSTRUCTOR)) {
            superCall->resolvedFunction = fd;
        }
    }

    CJC_NULLPTR_CHECK(varDecl);
    CreateConstructor(beBoxedType, cd, std::move(funcParam), std::move(superCall));
    CollectExtendedInterface(beBoxedType, cd, *varDecl);

    return cd;
}

void AutoBoxing::CreateConstructor(
    Ty& beBoxedType, const OwnedPtr<ClassDecl>& cd, OwnedPtr<FuncParam> funcParam, OwnedPtr<CallExpr> superCall)
{
    OwnedPtr<FuncBody> funcBody = MakeOwned<FuncBody>();
    funcBody->body = MakeOwned<Block>();
    funcBody->body->ty = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    funcBody->ty = typeManager.GetFunctionTy({&beBoxedType}, cd->ty);
    funcBody->EnableAttr(Attribute::COMPILER_ADD);

    auto funcParamList = MakeOwned<FuncParamList>();
    funcParamList->params.push_back(std::move(funcParam));
    funcBody->paramLists.push_back(std::move(funcParamList));
    funcBody->body->body.push_back(std::move(superCall));
    auto re = CreateReturnExpr(CreateUnitExpr(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)));
    re->refFuncBody = funcBody.get();
    funcBody->body->body.push_back(std::move(re));

    OwnedPtr<FuncDecl> initFunc = CreateFuncDecl("init", std::move(funcBody));
    initFunc->funcBody->parentClassLike = cd.get();
    initFunc->constructorCall = ConstructorCall::SUPER;
    initFunc->outerDecl = cd.get();
    initFunc->fullPackageName = cd->fullPackageName;
    initFunc->isConst = true;
    initFunc->EnableAttr(Attribute::CONSTRUCTOR, Attribute::IN_CLASSLIKE, Attribute::IMPLICIT_ADD, Attribute::INTERNAL);
    initFunc->linkage = Linkage::INTERNAL;
    cd->body->decls.push_back(std::move(initFunc));
}

namespace {
/**
 * If the boxed decl is class and has super class, the boxed decl also need to inherited all super interfaces
 * which is inherited by super class.
 * @param tyMgr [in] typeManager used to get type inherited interfaces.
 * @param id    [in] the inheritable decl of boxed base type.
 * @param cd   [out] the boxed decl which need to add inherited interface.
 */
void InsertSuperClassInheritedInterfaces(TypeManager& tyMgr, const InheritableDecl& id, ClassDecl& cd)
{
    if (id.astKind != ASTKind::CLASS_DECL) {
        return;
    }
    auto superClass = RawStaticCast<const ClassDecl*>(&id)->GetSuperClassDecl();
    if (!superClass || !superClass->ty) {
        return;
    }
    auto superInterfaces = tyMgr.GetAllSuperTys(*superClass->ty);
    std::set<Ptr<Ty>, CmpTyByName> sortedTys(superInterfaces.begin(), superInterfaces.end());
    for (auto& interfaceTy : sortedTys) {
        auto tyDecl = Ty::GetDeclOfTy<InheritableDecl>(interfaceTy);
        if (tyDecl && tyDecl->astKind == ASTKind::INTERFACE_DECL) {
            cd.inheritedTypes.push_back(CreateRefType(*tyDecl));
        }
    }
}

void CollectParamList(const FuncDecl& fd, const OwnedPtr<FuncBody>& funcBody, std::vector<OwnedPtr<FuncArg>>& args)
{
    for (auto& paramLists : fd.funcBody->paramLists) {
        auto paramList = MakeOwned<FuncParamList>();
        for (auto& param : paramLists->params) {
            auto funcParam = CreateFuncParam(param->identifier);
            funcParam->ty = param->ty;
            funcParam->isNamedParam = param->isNamedParam;
            auto argExpr = CreateRefExpr(param->identifier, param->ty);
            argExpr->ref.target = funcParam.get();
            auto arg = CreateFuncArgForOptional(*funcParam);
            arg->expr = std::move(argExpr);
            args.push_back(std::move(arg));
            paramList->params.push_back(std::move(funcParam));
        }
        funcBody->paramLists.push_back(std::move(paramList));
    }
}

void CollectFunctions(const std::vector<Ptr<Decl>>& decls, std::vector<Ptr<FuncDecl>>& methods)
{
    for (auto decl : decls) {
        // 1. Do not collect non-public function, which will not be called from boxed object as interface function.
        // 2. Do not collect generic member and constructors.
        bool ignored =
            !decl->TestAttr(Attribute::PUBLIC) || decl->TestAttr(Attribute::GENERIC) || IsClassOrEnumConstructor(*decl);
        if (ignored) {
            continue;
        } else if (auto pd = DynamicCast<PropDecl*>(decl); pd && !pd->TestAttr(Attribute::STATIC)) {
            for (auto& fd : pd->setters) {
                methods.emplace_back(fd.get());
            }
            for (auto& fd : pd->getters) {
                methods.emplace_back(fd.get());
            }
        } else if (auto fd = DynamicCast<FuncDecl*>(decl);
                   fd && !fd->TestAttr(Attribute::STATIC) && !fd->TestAttr(Attribute::CONSTRUCTOR)) {
            methods.emplace_back(fd);
        }
    }
}

void CloneInstanceFunctions(const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl, const std::vector<Ptr<Decl>>& decls)
{
    std::vector<Ptr<FuncDecl>> methods;
    // Collect box used functions.
    CollectFunctions(decls, methods);
    for (auto& fd : methods) {
        OwnedPtr<FuncBody> funcBody = MakeOwned<FuncBody>();
        funcBody->body = MakeOwned<Block>();
        funcBody->body->ty = fd->ty;
        funcBody->ty = fd->ty;
        funcBody->EnableAttr(Attribute::COMPILER_ADD);

        auto value = CreateRefExpr("$value");
        value->ref.target = &varDecl;
        value->ty = varDecl.ty;
        value->curFile = varDecl.curFile;

        auto memberAccess = CreateMemberAccess(std::move(value), fd->identifier);
        memberAccess->target = fd;
        memberAccess->ty = fd->ty;

        std::vector<OwnedPtr<FuncArg>> args;
        CollectParamList(*fd, funcBody, args);
        auto callExpr = CreateCallExpr(std::move(memberAccess), std::move(args));
        auto funcTy = StaticCast<FuncTy*>(funcBody->body->ty);
        callExpr->ty = funcTy->retTy;
        callExpr->callKind = CallKind::CALL_DECLARED_FUNCTION;
        callExpr->resolvedFunction = fd;

        auto re = CreateReturnExpr(std::move(callExpr), funcBody.get());
        re->ty = funcTy->retTy;
        funcBody->body->body.push_back(std::move(re));

        auto ret = CreateFuncDecl(fd->identifier, std::move(funcBody));
        ret->isConst = fd->isConst;
        ret->funcBody->parentClassLike = cd.get();
        ret->funcBody->EnableAttr(Attribute::IN_CLASSLIKE);
        ret->outerDecl = cd.get();
        ret->fullPackageName = cd->fullPackageName;
        // Interface's implementation function is always public.
        ret->EnableAttr(
            Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::NO_REFLECT_INFO, Attribute::IMPLICIT_ADD);
        ret->linkage = Linkage::INTERNAL;
        cd->body->decls.push_back(std::move(ret));
    }
}

/** Insert each method's signature from @p methods to @p methodSigs if not existed. */
void InsertFuncToSignatureMap(const std::vector<Ptr<FuncDecl>>& methods, FuncSig2Decl& methodSigs)
{
    for (auto fd : methods) {
        CJC_ASSERT(fd && fd->ty && fd->ty->kind == TypeKind::TYPE_FUNC);
        auto funcTy = RawStaticCast<FuncTy*>(fd->ty);
        auto keyPair = std::make_pair(fd->identifier, funcTy->paramTys);
        if (methodSigs.find(keyPair) == methodSigs.end()) {
            methodSigs.emplace(keyPair, fd);
        }
    }
}

/**
 * Collect all accessible public member of 'boxedDecl'.
 * NOTE: Since the member of extends cannot override or be overriden,
 *       extend members of current and super classes will be collected later.
 */
void CloneAllInstanceFunctions(const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl, InheritableDecl& boxedDecl)
{
    if (boxedDecl.astKind != ASTKind::CLASS_DECL) {
        CloneInstanceFunctions(cd, varDecl, boxedDecl.GetMemberDeclPtrs());
        return;
    }
    FuncSig2Decl methodSigs;
    std::vector<Ptr<FuncDecl>> currentMethods;
    auto curClass = StaticAs<ASTKind::CLASS_DECL>(&boxedDecl);
    do {
        CollectFunctions(curClass->GetMemberDeclPtrs(), currentMethods);
        InsertFuncToSignatureMap(currentMethods, methodSigs);
        currentMethods.clear();
        curClass = curClass->GetSuperClassDecl();
    } while (curClass != nullptr);

    std::vector<Ptr<Decl>> allMethods;
    allMethods.reserve(methodSigs.size());
    for (auto& it : std::as_const(methodSigs)) {
        allMethods.emplace_back(it.second);
    }
    CloneInstanceFunctions(cd, varDecl, allMethods);
}

void CloneInstanceFunctionsFromMap(const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl, const FuncSig2Decl& methodSigs)
{
    std::vector<Ptr<Decl>> allMethods;
    for (auto& it : std::as_const(methodSigs)) {
        // Collect newly found unimplemented interface functions.
        if (it.second->outerDecl != cd.get()) {
            allMethods.emplace_back(it.second);
        }
    }
    CloneInstanceFunctions(cd, varDecl, allMethods);
}
/**
 * Collect all instance function, compare them with all inherited interface of the abstract class.
 * Clone unimplemented interface functions to the boxed decl.
 */
void CloneUnimplementedInterfaceFunc(TypeManager& tyMgr, const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl)
{
    FuncSig2Decl methodSigs;
    std::vector<Ptr<FuncDecl>> currentMethods;
    CollectFunctions(cd->GetMemberDeclPtrs(), currentMethods);
    InsertFuncToSignatureMap(currentMethods, methodSigs);
    currentMethods.clear();

    for (auto& iTy : tyMgr.GetAllSuperInterfaceTysBFS(*cd)) {
        CJC_NULLPTR_CHECK(iTy);
        auto decl = Ty::GetDeclOfTy(iTy);
        if (decl && decl->astKind == ASTKind::INTERFACE_DECL) {
            CollectFunctions(decl->GetMemberDeclPtrs(), currentMethods);
            InsertFuncToSignatureMap(currentMethods, methodSigs);
            currentMethods.clear();
        }
    }
    CloneInstanceFunctionsFromMap(cd, varDecl, methodSigs);
}
} // namespace

void AutoBoxing::CollectSpecifiedInheritedType(
    const OwnedPtr<AST::ClassDecl>& cd, const std::string& pkgName, const std::string& typeName)
{
    auto tmp = CreateRefType(typeName);
    tmp->curFile = cd->curFile;
    if (auto decl = importManager.GetImportedDecl(pkgName, typeName)) {
        tmp->ref.target = decl;
        tmp->ty = tmp->ref.target->ty;
        cd->inheritedTypes.emplace_back(std::move(tmp));
    }
}

void AutoBoxing::CollectExtendedInterface(Ty& beBoxedType, const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl)
{
    std::set<Ptr<ExtendDecl>> tmpExtends;
    Ptr<InheritableDecl> decl = Ty::GetDeclOfTy<InheritableDecl>(&beBoxedType);
    if (decl) {
        // Clone inherited interface types and member decls of class/struct/enum decls.
        for (auto& interfaceTy : decl->GetSuperInterfaceTys()) {
            CJC_ASSERT(interfaceTy->decl);
            if (HasJavaAttr(*interfaceTy->decl)) {
                continue;
            }
            cd->inheritedTypes.push_back(CreateRefType(*interfaceTy->decl));
        }
        InsertSuperClassInheritedInterfaces(typeManager, *decl, *cd);
        CloneAllInstanceFunctions(cd, varDecl, *decl);
        tmpExtends = CollectAllRelatedExtends(typeManager, *decl);
    } else {
        tmpExtends = typeManager.GetBuiltinTyExtends(beBoxedType);
    }

    // CPointer, CString, CFunc, @C struct, and all the primitive types in cffi type-mapping sheet,
    // meet the CType constraint.
    // We can't add inherited type CType to the types themselves in cangjie code.
    // When we pass these subtypes as arguments to interface CType, we need to autobox.
    // So here we add an inheritance to the CType for the boxed type.
    if (Ty::IsMetCType(beBoxedType)) {
        CollectSpecifiedInheritedType(cd, CORE_PACKAGE_NAME, CTYPE_NAME);
    }

    CollectExtendedInterfaceHelper(cd, varDecl, tmpExtends);
    // Collect unimplemented interface function for abstract decl.
    if (decl && decl->TestAttr(Attribute::ABSTRACT)) {
        CloneUnimplementedInterfaceFunc(typeManager, cd, varDecl);
    }
}

void AutoBoxing::CollectExtendedInterfaceHelper(
    const OwnedPtr<ClassDecl>& cd, VarDecl& varDecl, const std::set<Ptr<ExtendDecl>>& extends) const
{
    // For bep, extends should be sorted by FullInheritedTy to make sequence stable.
    auto compare = [](Ptr<ExtendDecl> p1, Ptr<ExtendDecl> p2) {
        return p1->fullPackageName < p2->fullPackageName || GetFullInheritedTy(*p1) < GetFullInheritedTy(*p2);
    };
    std::set<Ptr<ExtendDecl>, std::function<bool(Ptr<ExtendDecl>, Ptr<ExtendDecl>)>> relatedExtends(compare);
    auto& dependencies = importManager.GetAllDependentPackageNames(cd->fullPackageName);
    for (auto e : extends) {
        /* Not add extend node when it is not be imported and not in current package.
         * When -module is used for compilation, imported decls does not have the IMPORTED
         * attribute.
         */
        CJC_ASSERT(e && e->curFile);
        if (e->fullPackageName != cd->fullPackageName &&
            (dependencies.count(e->fullPackageName) == 0 || !e->IsExportedDecl())) {
            continue;
        }
        relatedExtends.insert(e);
    }
    FuncSig2Decl methodSigs;
    // Collect current instance function in boxed decls for now.
    std::vector<Ptr<FuncDecl>> currentMethods;
    CollectFunctions(cd->GetMemberDeclPtrs(), currentMethods);
    InsertFuncToSignatureMap(currentMethods, methodSigs);
    std::unordered_set<Ptr<Decl>> collected;
    for (auto extend : relatedExtends) {
        if (collected.count(extend->genericDecl) > 0) {
            continue;
        }
        // Collect extended interfaces
        for (auto& i : extend->inheritedTypes) {
            auto iTy = StaticCast<InterfaceTy*>(i->ty);
            CJC_NULLPTR_CHECK(iTy->decl);
            auto iType = CreateRefType(*iTy->decl);
            cd->inheritedTypes.push_back(std::move(iType));
        }
        // Collect non-duplicate decls.
        currentMethods.clear();
        CollectFunctions(extend->GetMemberDeclPtrs(), currentMethods);
        InsertFuncToSignatureMap(currentMethods, methodSigs);
        if (extend->genericDecl) {
            collected.emplace(extend->genericDecl);
        }
    }
    // Clone instance functions without duplication.
    CloneInstanceFunctionsFromMap(cd, varDecl, methodSigs);
}

bool AutoBoxing::NeedBoxOption(Ty& child, Ty& target)
{
    if (Ty::IsInitialTy(&child) || Ty::IsInitialTy(&target) ||
        (typeManager.CheckTypeCompatibility(&child, &target, false, target.IsGeneric()) !=
            TypeCompatibility::INCOMPATIBLE) ||
        child.kind == TypeKind::TYPE_NOTHING || target.kind != TypeKind::TYPE_ENUM) {
        return false;
    }
    auto lCnt = CountOptionNestedLevel(child);
    auto rCnt = CountOptionNestedLevel(target);
    // If type contains generic ty, current is node inside @Java class. Otherwise, incompatible types need to be boxed.
    if (lCnt == rCnt && child.HasGeneric()) {
        return false;
    }
    auto enumTy = RawStaticCast<EnumTy*>(&target);
    if (enumTy->declPtr->fullPackageName != CORE_PACKAGE_NAME || enumTy->declPtr->identifier != STD_LIB_OPTION) {
        return false;
    }
    return true;
}

// Option Box happens twice before and after instantiation, and must before extend box.
void AutoBoxing::TryOptionBox(EnumTy& target, Expr& expr)
{
    if (expr.ty && target.typeArgs[0] && NeedBoxOption(*expr.ty, *target.typeArgs[0])) {
        TryOptionBox(*StaticCast<EnumTy*>(target.typeArgs[0]), expr);
    }
    auto ed = target.decl;
    Ptr<FuncDecl> optionDecl = nullptr;
    for (auto& it : ed->constructors) {
        if (it->identifier == OPTION_VALUE_CTOR) {
            optionDecl = StaticCast<FuncDecl*>(it.get());
            break;
        }
    }
    if (optionDecl == nullptr) {
        return;
    }

    auto baseFunc = CreateRefExpr(OPTION_VALUE_CTOR);
    baseFunc->EnableAttr(Attribute::IMPLICIT_ADD);
    baseFunc->ref.target = optionDecl;
    baseFunc->ty =
        typeManager.GetInstantiatedTy(optionDecl->ty, GenerateTypeMapping(*ed, target.typeArgs));

    std::vector<OwnedPtr<FuncArg>> arg;
    if (expr.desugarExpr) {
        arg.emplace_back(CreateFuncArg(std::move(expr.desugarExpr)));
    } else {
        arg.emplace_back(CreateFuncArg(ASTCloner::Clone(Ptr(&expr))));
    }

    auto ce = CreateCallExpr(std::move(baseFunc), std::move(arg));
    ce->callKind = AST::CallKind::CALL_DECLARED_FUNCTION;
    ce->ty = &target;
    ce->resolvedFunction = optionDecl;
    if (expr.astKind == ASTKind::BLOCK) {
        // For correct deserialization, we need to keep type of block.
        auto b = MakeOwnedNode<Block>();
        b->ty = ce->ty;
        b->body.emplace_back(std::move(ce));
        expr.desugarExpr = std::move(b);
    } else {
        expr.desugarExpr = std::move(ce);
    }
    AddCurFile(*expr.desugarExpr, expr.curFile);
    expr.ty = expr.desugarExpr->ty;
}

/**
 * Option Box happens before type check finished with no errors.
 * All nodes and sema types should be valid.
 */
void AutoBoxing::AddOptionBox(Package& pkg)
{
    std::function<VisitAction(Ptr<Node>)> preVisit = [this](Ptr<Node> node) -> VisitAction {
        return match(*node)([this](const VarDecl& vd) { return AddOptionBoxHandleVarDecl(vd); },
            [this](const AssignExpr& ae) { return AddOptionBoxHandleAssignExpr(ae); },
            [this](CallExpr& ce) { return AddOptionBoxHandleCallExpr(ce); },
            [this](const IfExpr& ie) { return AddOptionBoxHandleIfExpr(ie); },
            [this](TryExpr& te) { return AddOptionBoxHandleTryExpr(te); },
            [this](const ReturnExpr& re) { return AddOptionBoxHandleReturnExpr(re); },
            [this](ArrayLit& lit) { return AddOptionBoxHandleArrayLit(lit); },
            [this](MatchExpr& me) { return AddOptionBoxHandleMatchExpr(me); },
            [this](const TupleLit& tl) { return AddOptionBoxHandleTupleList(tl); },
            [this](ArrayExpr& ae) { return AddOptionBoxHandleArrayExpr(ae); },
            []() { return VisitAction::WALK_CHILDREN; });
    };
    Walker walker(&pkg, preVisit);
    walker.Walk();
}

VisitAction AutoBoxing::AddOptionBoxHandleTupleList(const TupleLit& tl)
{ // Tuple literal allows element been boxed.
    auto tupleTy = DynamicCast<TupleTy*>(tl.ty);
    if (tupleTy == nullptr) {
        return VisitAction::WALK_CHILDREN;
    }
    auto typeArgs = tupleTy->typeArgs;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (tl.children[i]->ty && typeArgs[i] && NeedBoxOption(*tl.children[i]->ty, *typeArgs[i])) {
            TryOptionBox(*StaticCast<EnumTy*>(typeArgs[i]), *tl.children[i]);
        }
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleMatchExpr(MatchExpr& me)
{
    for (auto& single : me.matchCases) {
        CJC_ASSERT(me.ty && single->exprOrDecls);
        AddOptionBoxHandleBlock(*single->exprOrDecls, *me.ty);
    }
    for (auto& caseOther : me.matchCaseOthers) {
        CJC_ASSERT(me.ty && caseOther->exprOrDecls);
        AddOptionBoxHandleBlock(*caseOther->exprOrDecls, *me.ty);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleArrayLit(ArrayLit& lit)
{
    if (Ty::IsInitialTy(lit.ty) || !lit.ty->IsStructArray()) {
        return VisitAction::WALK_CHILDREN;
    }

    if (lit.ty->typeArgs.size() == 1) {
        auto targetTy = lit.ty->typeArgs[0];
        CJC_NULLPTR_CHECK(targetTy);
        for (auto& child : lit.children) {
            if (child->ty && NeedBoxOption(*child->ty, *targetTy)) {
                TryOptionBox(*StaticCast<EnumTy*>(targetTy), *child);
            }
        }
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleIfExpr(const IfExpr& ie)
{
    if (!Ty::IsTyCorrect(ie.ty) || ie.ty->IsUnitOrNothing() || !ie.thenBody) {
        return VisitAction::WALK_CHILDREN;
    }
    AddOptionBoxHandleBlock(*ie.thenBody, *ie.ty);
    if (ie.hasElse && ie.elseBody) {
        if (auto block = DynamicCast<Block*>(ie.elseBody.get()); block) {
            AddOptionBoxHandleBlock(*block, *ie.ty);
        }
    }
    return VisitAction::WALK_CHILDREN;
}

void AutoBoxing::AddOptionBoxHandleBlock(Block& block, Ty& ty)
{
    // If the block is empty or end with declaration, the last type is 'Unit',
    // otherwise the last type is the type of last expression.
    auto lastExprOrDecl = block.GetLastExprOrDecl();
    Ptr<Ty> lastTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    if (auto expr = DynamicCast<Expr*>(lastExprOrDecl)) {
        lastTy = expr->ty;
    }
    if (!lastTy || !NeedBoxOption(*lastTy, ty)) {
        return;
    }
    // If the block is empty or end with declaration, we need to insert a unitExpr for box.
    if (Is<Decl>(lastExprOrDecl) || block.body.empty()) {
        auto unitExpr = CreateUnitExpr(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
        unitExpr->curFile = block.curFile;
        lastExprOrDecl = unitExpr.get();
        block.body.emplace_back(std::move(unitExpr));
    }

    if (auto lastExpr = DynamicCast<Expr*>(lastExprOrDecl)) {
        TryOptionBox(StaticCast<EnumTy&>(ty), *lastExpr);
        block.ty = lastExpr->ty;
    }
}

VisitAction AutoBoxing::AddOptionBoxHandleTryExpr(TryExpr& te)
{
    if (!Ty::IsTyCorrect(te.ty)) {
        return VisitAction::WALK_CHILDREN;
    }
    if (te.tryBlock) {
        AddOptionBoxHandleBlock(*te.tryBlock, *te.ty);
    }
    for (auto& ce : te.catchBlocks) {
        AddOptionBoxHandleBlock(*ce, *te.ty);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleArrayExpr(ArrayExpr& ae)
{
    bool ignore = !Ty::IsTyCorrect(ae.ty) || ae.initFunc || ae.args.size() < 1;
    if (ignore) {
        return VisitAction::WALK_CHILDREN;
    }
    auto targetTy = typeManager.GetTypeArgs(*ae.ty)[0];

    Ptr<FuncArg> arg = nullptr;
    if (ae.isValueArray) {
        // For VArray only one argument, and it need option box.
        arg = ae.args[0].get();
    } else if (ae.args.size() > 1) {
        // For RawArray(size, item:T) boxing argIndex is 1, only this case may need option box.
        arg = ae.args[1].get();
    }
    if (arg && arg->expr && arg->expr->ty && targetTy && NeedBoxOption(*arg->expr->ty, *targetTy)) {
        TryOptionBox(*StaticCast<EnumTy*>(targetTy), *arg->expr);
        arg->ty = arg->expr->ty;
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleCallExpr(CallExpr& ce)
{
    bool ignored = !ce.baseFunc || !ce.baseFunc->ty || ce.baseFunc->ty->kind != TypeKind::TYPE_FUNC;
    if (ignored) {
        return VisitAction::WALK_CHILDREN;
    }
    auto funcTy = RawStaticCast<FuncTy*>(ce.baseFunc->ty);
    unsigned count = 0;
    auto callCheck = [&count, &funcTy, this](auto begin, auto end) {
        for (auto it = begin; it != end; ++it) {
            if (count >= funcTy->paramTys.size()) {
                break;
            }
            auto paramTy = funcTy->paramTys[count];
            // It's possible that childs have different box type, so does not break after match.
            if ((*it)->expr && (*it)->expr->ty && paramTy && NeedBoxOption(*(*it)->expr->ty, *paramTy)) {
                TryOptionBox(*StaticCast<EnumTy*>(paramTy), *(*it)->expr);
                (*it)->ty = (*it)->expr->ty;
            }
            ++count;
        }
    };
    if (ce.desugarArgs.has_value()) {
        callCheck(ce.desugarArgs->begin(), ce.desugarArgs->end());
    } else {
        callCheck(ce.args.begin(), ce.args.end());
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleAssignExpr(const AssignExpr& ae)
{
    if (ae.desugarExpr) {
        return VisitAction::WALK_CHILDREN;
    }
    if (ae.rightExpr->ty && ae.leftValue->ty && NeedBoxOption(*ae.rightExpr->ty, *ae.leftValue->ty)) {
        TryOptionBox(*StaticCast<EnumTy*>(ae.leftValue->ty), *ae.rightExpr);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleVarDecl(const VarDecl& vd)
{
    if (vd.initializer && vd.initializer->ty && vd.ty && NeedBoxOption(*vd.initializer->ty, *vd.ty)) {
        TryOptionBox(*StaticCast<EnumTy*>(vd.ty), *vd.initializer);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleReturnExpr(const ReturnExpr& re)
{
    if (re.expr && re.refFuncBody && re.refFuncBody->ty && re.refFuncBody->ty->kind == TypeKind::TYPE_FUNC) {
        auto funcTy = RawStaticCast<FuncTy*>(re.refFuncBody->ty);
        if (re.expr->ty && funcTy->retTy && NeedBoxOption(*re.expr->ty, *funcTy->retTy)) {
            auto expr = re.desugarExpr ? re.desugarExpr.get() : re.expr.get();
            TryOptionBox(*StaticCast<EnumTy*>(funcTy->retTy), *expr);
        }
    }
    return VisitAction::WALK_CHILDREN;
}
