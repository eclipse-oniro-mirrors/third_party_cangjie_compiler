// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements the MergeCusAnno.
 */

#include "MergeAnnoFromCjd.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/PrintNode.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Utils/Casting.h"
#include "cangjie/Utils/ICEUtil.h"
#include "cangjie/Utils/SafePointer.h"

using namespace Cangjie;
using namespace Cangjie::AST;

namespace {
bool IsSameType(Ptr<Type> lt, Ptr<Ty> rty);
bool IsSameType(Ptr<Type> lt, Ptr<Type> rt)
{
    CJC_ASSERT(Ty::IsTyCorrect(rt->ty));
    switch (lt->astKind) {
        case ASTKind::REF_TYPE: {
            auto lrt = StaticCast<RefType>(lt);
            if (rt->astKind != ASTKind::REF_TYPE) {
                return false;
            }
            auto rrtName = rt->ty->IsPrimitive() ? rt->ty->String() : rt->ty->name;
            if (lrt->ref.identifier.Val() != rrtName) {
                return false;
            }
            if (lrt->typeArguments.size() != rt->ty->typeArgs.size()) {
                return false;
            }
            for (size_t i = 0; i < lrt->typeArguments.size(); ++i) {
                if (!IsSameType(lrt->typeArguments[i].get(), rt->ty->typeArgs[i])) {
                    return false;
                }
            }
            break;
        }
        case ASTKind::QUALIFIED_TYPE: {
            auto lqt = StaticCast<QualifiedType>(lt);
            if (rt->astKind != ASTKind::QUALIFIED_TYPE) {
                return false;
            }
            auto rqt = StaticCast<QualifiedType>(rt);
            if (lqt->field.Val() != rqt->field.Val()) {
                return false;
            }
            if (!IsSameType(lqt->baseType.get(), rqt->baseType.get())) {
                return false;
            }
            break;
        }
        case ASTKind::FUNC_TYPE: {
            auto lft = StaticCast<FuncType>(lt);
            if (rt->astKind != ASTKind::FUNC_TYPE) {
                return false;
            }
            auto rft = StaticCast<FuncType>(rt);
            if (lft->paramTypes.size() != rft->paramTypes.size()) {
                return false;
            }
            for (size_t i = 0; i < lft->paramTypes.size(); ++i) {
                if (!IsSameType(lft->paramTypes[i].get(), rft->paramTypes[i].get())) {
                    return false;
                }
            }
            if (!IsSameType(lft->retType.get(), rft->retType.get())) {
                return false;
            }
            break;
        }
        case ASTKind::TUPLE_TYPE: {
            auto ltt = StaticCast<TupleType>(lt);
            if (rt->astKind != ASTKind::TUPLE_TYPE) {
                return false;
            }
            auto rtt = StaticCast<TupleType>(rt);
            if (ltt->fieldTypes.size() != rtt->fieldTypes.size()) {
                return false;
            }
            for (size_t i = 0; i < ltt->fieldTypes.size(); ++i) {
                if (!IsSameType(ltt->fieldTypes[i].get(), rtt->fieldTypes[i].get())) {
                    return false;
                }
            }
            break;
        }
        case ASTKind::OPTION_TYPE: {
            auto lot = StaticCast<OptionType>(lt);
            if (rt->astKind != ASTKind::OPTION_TYPE) {
                return false;
            }
            auto rot = StaticCast<OptionType>(rt);
            if (!IsSameType(lot->componentType.get(), rot->componentType.get())) {
                return false;
            }
            break;
        }
        case ASTKind::VARRAY_TYPE: {
            auto lat = StaticCast<VArrayType>(lt);
            if (rt->astKind != ASTKind::VARRAY_TYPE) {
                return false;
            }
            auto rat = StaticCast<VArrayType>(rt);
            if (!IsSameType(lat->typeArgument.get(), rat->typeArgument.get())) {
                return false;
            }
            if (!IsSameType(lat->constantType.get(), rat->constantType.get())) {
                return false;
            }
            break;
        }
        case ASTKind::CONSTANT_TYPE: {
            auto lct = StaticCast<ConstantType>(lt);
            if (rt->astKind != ASTKind::CONSTANT_TYPE) {
                return false;
            }
            auto rct = StaticCast<ConstantType>(rt);
            auto lce = DynamicCast<LitConstExpr>(lct->constantExpr.get());
            auto rce = DynamicCast<LitConstExpr>(rct->constantExpr.get());
            if ((lce == nullptr) != (rce == nullptr)) {
                return false;
            }
            if (!lce) {
                return true;
            }
            if (lce->kind != rce->kind || lce->stringValue != rce->stringValue) {
                return false;
            }
            break;
        }
        case ASTKind::PRIMITIVE_TYPE: {
            auto lpt = StaticCast<PrimitiveType>(lt);
            auto lptName = lpt->str;
            auto rtyName = rt->ty->IsPrimitive() ? rt->ty->String() : rt->ty->name;
            lptName = lptName == "Rune" ? "UInt8" : lptName;
            rtyName = rtyName == "Rune" ? "UInt8" : rtyName;
            if (lptName != rtyName) {
                return false;
            }
            if (lpt->kind != rt->ty->kind) {
                return false;
            }
            break;
        }
        default:
            break;
    }
    return true;
}

bool IsSameType(Ptr<Type> lt, Ptr<Ty> rty)
{
    switch (lt->astKind) {
        case ASTKind::REF_TYPE: {
            auto lrt = StaticCast<RefType>(lt);
            auto rtyName = rty->IsPrimitive() ? rty->String() : rty->name;
            // If the identifier is the name after the type alias, the comparison will not succeed.
            if (lrt->ref.identifier.Val() != rtyName) {
                return false;
            }
            if (lrt->typeArguments.size() != rty->typeArgs.size()) {
                return false;
            }
            for (size_t i = 0; i < lrt->typeArguments.size(); ++i) {
                if (!IsSameType(lrt->typeArguments[i].get(), rty->typeArgs[i])) {
                    return false;
                }
            }
            break;
        }
        case ASTKind::QUALIFIED_TYPE: {
            auto lqt = StaticCast<QualifiedType>(lt);
            if (lqt->field.Val() != rty->name) {
                return false;
            }
            break;
        }
        case ASTKind::FUNC_TYPE: {
            if (!rty->IsFunc()) {
                return false;
            }
            auto lft = StaticCast<FuncType>(lt);
            auto rfty = StaticCast<FuncTy>(rty);
            if (lft->paramTypes.size() != rfty->paramTys.size()) {
                return false;
            }
            for (size_t i = 0; i < lft->paramTypes.size(); ++i) {
                if (!IsSameType(lft->paramTypes[i].get(), rfty->paramTys[i])) {
                    return false;
                }
            }
            if (!IsSameType(lft->retType.get(), rfty->retTy)) {
                return false;
            }
            break;
        }
        case ASTKind::TUPLE_TYPE: {
            if (!rty->IsTuple()) {
                return false;
            }
            auto ltt = StaticCast<TupleType>(lt);
            auto rtty = StaticCast<TupleTy>(rty);
            if (ltt->fieldTypes.size() != rtty->typeArgs.size()) {
                return false;
            }
            for (size_t i = 0; i < ltt->fieldTypes.size(); ++i) {
                if (!IsSameType(ltt->fieldTypes[i].get(), rtty->typeArgs[i].get())) {
                    return false;
                }
            }
            break;
        }
        case ASTKind::OPTION_TYPE: {
            if (!rty->IsCoreOptionType()) {
                return false;
            }
            auto lot = StaticCast<OptionType>(lt);
            auto roty = StaticCast<EnumTy>(rty);
            if (!IsSameType(lot->componentType.get(), roty->typeArgs[0])) {
                return false;
            }
            break;
        }
        case ASTKind::VARRAY_TYPE: {
            if (rty->kind != TypeKind::TYPE_VARRAY) {
                return false;
            }
            auto lat = StaticCast<VArrayType>(lt);
            auto rat = StaticCast<VArrayTy>(rty);
            if (!IsSameType(lat->typeArgument.get(), rat->typeArgs[0].get())) {
                return false;
            }
            auto lct = DynamicCast<ConstantType>(lat->constantType.get());
            CJC_NULLPTR_CHECK(lct);
            auto lce = DynamicCast<LitConstExpr>(lct->constantExpr.get());
            CJC_NULLPTR_CHECK(lce);
            if (lce->stringValue != std::to_string(rat->size)) {
                return false;
            }
            break;
        }
        case ASTKind::PRIMITIVE_TYPE: {
            if (!rty->IsPrimitive()) {
                return false;
            }
            auto lpt = StaticCast<PrimitiveType>(lt);
            auto rpty = StaticCast<PrimitiveTy>(rty);
            if (lpt->str != rpty->String()) {
                return false;
            }
            break;
        }
        default:
            break;
    }
    return true;
}

bool IsSameFuncByIdentifier(Ptr<FuncBody> lb, Ptr<FuncBody> rb)
{
    if ((lb->generic == nullptr) != (rb->generic == nullptr)) {
        return false;
    }
    if (lb->generic) {
        if (lb->generic->typeParameters.size() != rb->generic->typeParameters.size()) {
            return false;
        }
        for (size_t i = 0; i < lb->generic->typeParameters.size(); ++i) {
            if (lb->generic->typeParameters[i]->identifier.Val() != rb->generic->typeParameters[i]->identifier.Val()) {
                return false;
            }
        }
    }
    if (lb->paramLists[0]->params.size() != rb->paramLists[0]->params.size()) {
        return false;
    }
    for (size_t i = 0; i < lb->paramLists[0]->params.size(); ++i) {
        if (lb->paramLists[0]->params[i]->identifier.Val() != rb->paramLists[0]->params[i]->identifier.Val()) {
            return false;
        }
        // The type in the rb may be empty because the imported declaration does not contain the type node.
        bool isSameType = rb->paramLists[0]->params[i]->type
            ? IsSameType(lb->paramLists[0]->params[i]->type.get(), rb->paramLists[0]->params[i]->type.get())
            : IsSameType(lb->paramLists[0]->params[i]->type.get(), rb->paramLists[0]->params[i]->ty);
        if (!isSameType) {
            return false;
        }
    }
    return true;
}

bool IsSameDeclByIdentifier(Ptr<Decl> l, Ptr<Decl> r);

bool IsSamePattern(Ptr<Pattern> lp, Ptr<Decl> rd)
{
    switch (lp->astKind) {
        case ASTKind::VAR_PATTERN: {
            auto lvp = StaticCast<VarPattern>(lp);
            if (rd->astKind != ASTKind::VAR_DECL) {
                return false;
            }
            auto rvd = StaticCast<VarDecl>(rd);
            if (!IsSameDeclByIdentifier(lvp->varDecl.get(), rvd)) {
                return false;
            }
            break;
        }
        case ASTKind::TUPLE_PATTERN: {
            auto ltp = StaticCast<TuplePattern>(lp);
            bool matched = false;
            for (size_t i = 0; i < ltp->patterns.size(); ++i) {
                if (IsSamePattern(ltp->patterns[i].get(), rd)) {
                    matched = true;
                    break;
                }
            }
            return matched;
        }
        case ASTKind::ENUM_PATTERN: {
            auto lep = StaticCast<EnumPattern>(lp);
            bool matched = false;
            for (size_t i = 0; i < lep->patterns.size(); ++i) {
                if (IsSamePattern(lep->patterns[i].get(), rd)) {
                    matched = true;
                    break;
                }
            }
            return matched;
        }
        case ASTKind::VAR_OR_ENUM_PATTERN: {
            auto lvep = StaticCast<VarOrEnumPattern>(lp);
            if (!IsSamePattern(lvep->pattern.get(), rd)) {
                return false;
            }
            break;
        }
        case ASTKind::TYPE_PATTERN:
        case ASTKind::EXCEPT_TYPE_PATTERN:
        default:
            break;
    }
    return true;
}

// NOTE: l from .cj.d, r from .cjo.
bool IsSameDeclByIdentifier(Ptr<Decl> l, Ptr<Decl> r)
{
    if (l->astKind == ASTKind::VAR_WITH_PATTERN_DECL) {
        return IsSamePattern(StaticCast<VarWithPatternDecl>(l)->irrefutablePattern.get(), r);
    }
    auto lId = l->astKind == ASTKind::PRIMARY_CTOR_DECL ? "init" : l->identifier.Val();
    auto lKind = l->astKind == ASTKind::PRIMARY_CTOR_DECL ? ASTKind::FUNC_DECL : l->astKind;
    bool fastQuit =
        lId != r->identifier.Val() || lKind != r->astKind || (l->generic == nullptr) != (r->generic == nullptr);
    if (fastQuit) {
        return false;
    }
    if (l->generic) {
        if (l->generic->typeParameters.size() != r->generic->typeParameters.size()) {
            return false;
        }
        for (size_t i = 0; i < l->generic->typeParameters.size(); ++i) {
            if (l->generic->typeParameters[i]->identifier.Val() != r->generic->typeParameters[i]->identifier.Val()) {
                return false;
            }
        }
        // The extension declaration inserts the upper bound one more time.
        if (l->generic->genericConstraints.empty() != r->generic->genericConstraints.empty()) {
            return false;
        }
        if (l->generic->genericConstraints.size() > r->generic->genericConstraints.size()) {
            return false;
        }
        auto& lgc = l->generic->genericConstraints;
        auto& rgc = r->generic->genericConstraints;
        for (size_t i = 0; i < l->generic->genericConstraints.size(); ++i) {
            if (!IsSameType(lgc[i]->type.get(), rgc[i]->type.get())) {
                return false;
            }
            if (lgc[i]->upperBounds.size() != rgc[i]->upperBounds.size()) {
                return false;
            }
            for (size_t j = 0; j < l->generic->genericConstraints.size(); ++j) {
                if (!IsSameType(lgc[i]->upperBounds[j].get(), rgc[i]->upperBounds[j].get())) {
                    return false;
                }
            }
        }
    }
    switch (l->astKind) {
        case ASTKind::FUNC_DECL: {
            auto lfd = StaticCast<FuncDecl>(l);
            auto rfd = StaticCast<FuncDecl>(r);
            if (!IsSameFuncByIdentifier(lfd->funcBody.get(), rfd->funcBody.get())) {
                return false;
            }
            break;
        }
        case ASTKind::PRIMARY_CTOR_DECL: {
            auto pcd = StaticCast<PrimaryCtorDecl>(l);
            auto fd = StaticCast<FuncDecl>(r);
            if (!IsSameFuncByIdentifier(pcd->funcBody.get(), fd->funcBody.get())) {
                return false;
            }
            break;
        }
        case ASTKind::EXTEND_DECL: {
            auto led = StaticCast<ExtendDecl>(l);
            auto red = StaticCast<ExtendDecl>(r);
            if (!IsSameType(led->extendedType.get(), red->extendedType.get())) {
                return false;
            }
            if (led->inheritedTypes.size() != red->inheritedTypes.size()) {
                return false;
            }
            for (size_t i = 0; i < led->inheritedTypes.size(); ++i) {
                if (!IsSameType(led->inheritedTypes[i].get(), red->inheritedTypes[i].get())) {
                    return false;
                }
            }
            break;
        }
        case ASTKind::MACRO_EXPAND_DECL: {
            auto lmed = StaticCast<MacroExpandDecl>(l);
            auto rmed = StaticCast<MacroExpandDecl>(r);
            if (lmed->invocation.fullName != rmed->invocation.fullName ||
                lmed->invocation.identifier != rmed->invocation.identifier) {
                return false;
            }
            break;
        }
        case ASTKind::VAR_DECL:
        case ASTKind::PROP_DECL: {
            auto lvd = StaticCast<VarDecl>(l);
            auto rvd = StaticCast<VarDecl>(r);
            if (lvd->type) {
                bool isSameType =
                    rvd->type ? IsSameType(lvd->type.get(), rvd->type.get()) : IsSameType(lvd->type.get(), rvd->ty);
                if (!isSameType) {
                    return false;
                }
            }
            break;
        }
        default:
            break;
    }
    return true;
}

void FullMoveCheck(Ptr<Package> source)
{
    auto checkHasUnmovedAnno = [](Ptr<Node> node) {
        if (auto d = DynamicCast<Decl>(node); d && !d->annotations.empty()) {
            Debugln(d->identifier + "'s annotation is not moved.");
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(source, checkHasUnmovedAnno).Walk();
}
} // namespace

void Cangjie::MergeCusAnno(DiagnosticEngine& diag, Ptr<Package> target, Ptr<Package> source)
{
    // 1. Toplevel decls:
    //   1) Type declarations (class, struct, enum, interface)
    //   2) Extension declarations
    //   3) Top-level function declarations
    //   4) Top-level variable declarations
    // source to target, source from '.cj.d', target from '.cjo'.
    std::unordered_map<Ptr<Decl>, Ptr<Decl>> topDeclMapping;
    for (auto& file : source->files) {
        for (auto& toplevelDecl : file->decls) {
            if (toplevelDecl->astKind == ASTKind::MAIN_DECL || toplevelDecl->annotations.empty()) {
                continue;
            }
            topDeclMapping.emplace(toplevelDecl, nullptr);
        }
    }
    for (auto& file : target->files) {
        for (auto& toplevelDecl : file->decls) {
            // Short circuit.
            if (toplevelDecl->astKind == ASTKind::BUILTIN_DECL || !toplevelDecl->IsExportedDecl()) {
                continue;
            }
            auto found = std::find_if(topDeclMapping.begin(), topDeclMapping.end(), [&toplevelDecl](auto l) {
                if (l.second) {
                    return false;
                }
                return IsSameDeclByIdentifier(l.first, toplevelDecl);
            });
            if (found != topDeclMapping.end()) {
                found->second = toplevelDecl;
                for (auto& anno : found->first->annotations) {
                    found->second->annotations.emplace_back(std::move(anno));
                }
                found->first->annotations.clear();
            } else {
                auto identifier = toplevelDecl->astKind == ASTKind::EXTEND_DECL ? "extend " + toplevelDecl->ty->String()
                                                                                : toplevelDecl->identifier.Val();
                auto filePath = toplevelDecl->curFile ? toplevelDecl->curFile->filePath : "not in any file";
                filePath = filePath + ":" + std::to_string(toplevelDecl->begin.line);
                Debugln("[apilevel]", identifier, filePath, "not find in cj.d");
            }
        }
    }
    for (auto declPair : topDeclMapping) {
        if (!declPair.second) {
            auto identifier = declPair.first->astKind == ASTKind::EXTEND_DECL
                ? "extend at " + declPair.first->begin.ToString()
                : declPair.first->identifier.Val();
            Debugln("[apilevel]", identifier, "in '.cj.d' but not in '.cj'");
            continue;
        }
        // 2. Member
        //   1) Constructor declarations
        //   2) Member function declarations
        //   3) Member variable declarations
        //   4) Member property declarations
        //   5) Enum constructor declarations
        std::unordered_map<Ptr<Decl>, Ptr<Decl>> memberMapping;
        for (auto& member : declPair.first->GetMemberDeclPtrs()) {
            memberMapping.emplace(member, nullptr);
        }
        for (auto& member : declPair.second->GetMemberDeclPtrs()) {
            auto found = std::find_if(memberMapping.begin(), memberMapping.end(),
                [&member](auto l) { return IsSameDeclByIdentifier(l.first, member); });
            if (found != memberMapping.end()) {
                found->second = member;
                for (auto& anno : found->first->annotations) {
                    found->second->annotations.emplace_back(std::move(anno));
                }
                found->first->annotations.clear();
            } else {
                Debugln("[apilevel] member", member->identifier, "not find in .cj.d");
            }
        }
        // 3. Parameters in member functions/constructors
        for (auto memberPair : memberMapping) {
            if (!memberPair.second) {
                Debugln("[apilevel]", memberPair.first->identifier.Val(), "in '.cj.d' but not in '.cj'");
                continue;
            }
            if (!memberPair.first->IsFunc()) {
                continue;
            }
            auto s = StaticCast<FuncDecl>(memberPair.first);
            auto t = StaticCast<FuncDecl>(memberPair.second);
            for (size_t i = 0; i < s->funcBody->paramLists[0]->params.size(); ++i) {
                for (auto& anno : s->funcBody->paramLists[0]->params[i]->annotations) {
                    t->funcBody->paramLists[0]->params[i]->annotations.emplace_back(std::move(anno));
                }
                s->funcBody->paramLists[0]->params[i]->annotations.clear();
            }
        }
    }
    if (diag.GetErrorCount() == 0) {
        FullMoveCheck(source);
    }
}
