// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements AST and related data print apis.
 */

#include "cangjie/AST/PrintNode.h"

#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT)
#include <iostream>
#include <string>

#include "cangjie/AST/ASTCasting.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Basic/Print.h"
#include "cangjie/Basic/StringConvertor.h"
#include "cangjie/Utils/Utils.h"
#endif

namespace Cangjie {
using namespace AST;
// Enable printNode for debug and CI version.
#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT)
using namespace Utils;
using namespace Meta;

namespace {
const unsigned ONE_INDENT = 1u;
const unsigned TWO_INDENT = 2u;
const unsigned THREE_INDENT = 3u;
#define UNKNOWN_TY (ANSI_COLOR_RED + "unknown" + ANSI_COLOR_RESET)

void PrintAnnotations(const Decl& decl, unsigned indent)
{
    for (auto& anno : decl.annotations) {
        auto str = "Annotation: " + anno->identifier;
        PrintIndent(indent + ONE_INDENT, str);
    }
}

void PrintModifiers(const Decl& decl, unsigned indent)
{
    std::string str = "Modifiers:";
    for (auto& i : decl.modifiers) {
        str += " " + std::string(TOKENS[static_cast<int>(i.modifier)]);
    }
    if (!decl.modifiers.empty()) {
        PrintIndent(indent + ONE_INDENT, str);
    }
}

void PrintTy(unsigned indent, const std::string& str)
{
    PrintIndent(indent, "*Ty:", str);
}

void PrintIndentTokens(unsigned indent, const std::vector<Token>& args)
{
    PrintIndentOnly(indent);
    for (auto& it : args) {
        if (it.kind == TokenKind::NL) {
            Println();
            PrintIndentOnly(indent);
        } else {
            Print(it.Value());
        }
    }
    Println();
}

void PrintMacroInvocation(unsigned indent, const MacroInvocation& invocation)
{
    if (invocation.attrs.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no attributes");
    } else {
        PrintIndent(indent + ONE_INDENT, "# attributes", "{");
        PrintIndentTokens(indent + TWO_INDENT, invocation.attrs);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (invocation.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no arguments");
    } else {
        PrintIndent(indent + ONE_INDENT, "# arguments", "{");
        PrintIndentTokens(indent + TWO_INDENT, invocation.args);
        PrintIndent(indent + ONE_INDENT, "}");
    }
}

void PrintPackage(unsigned indent, const Package& package)
{
    PrintIndent(indent, "Package", package.fullPackageName, "{");
    for (auto& it : package.files) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintFile(unsigned indent, const File& file)
{
    PrintIndent(indent, "File", file.fileName, "{");
    PrintNode(file.package.get(), indent + ONE_INDENT, "package");
    for (auto& it : file.imports) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    for (auto& it : file.decls) {
        if (it->identifier == MAIN_INVOKE) {
            continue;
        }
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandParam(unsigned indent, const AST::MacroExpandParam& macroExpand)
{
    PrintIndent(indent, "MacroExpand:", macroExpand.invocation.fullName, "{");
    PrintMacroInvocation(indent, macroExpand.invocation);
    PrintIndent(indent, "}");
}

void PrintFuncParam(unsigned indent, const FuncParam& param, std::string& tyStr)
{
    PrintIndent(indent, "FuncParam", param.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintAnnotations(param, indent);
    PrintNode(param.type.get(), indent + ONE_INDENT, "type");
    PrintNode(param.assignment.get(), indent + ONE_INDENT, "assignment");
    PrintIndent(indent, "}");
}

void PrintFuncParamList(unsigned indent, const FuncParamList& paramList)
{
    PrintIndent(indent, "ParamList", "{");
    if (paramList.params.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no Params");
    } else {
        for (auto& it : paramList.params) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintIndent(indent, "}");
}

void PrintMacroDecl(unsigned indent, const MacroDecl& macroDecl, std::string& tyStr)
{
    PrintIndent(indent, "MacroDecl", macroDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintModifiers(macroDecl, indent);
    if (macroDecl.desugarDecl) {
        PrintIndent(indent + ONE_INDENT, "# desugar decl");
        PrintNode(macroDecl.desugarDecl.get(), indent + ONE_INDENT);
    } else {
        PrintIndent(indent + ONE_INDENT, "# macro body");
        PrintNode(macroDecl.funcBody.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintFuncDecl(unsigned indent, const FuncDecl& funcDecl, std::string& tyStr)
{
    PrintIndent(indent, "FuncDecl", funcDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (funcDecl.TestAttr(Attribute::CONSTRUCTOR) && funcDecl.constructorCall == ConstructorCall::SUPER) {
        if (auto ce = DynamicCast<CallExpr*>(funcDecl.funcBody->body->body.begin()->get()); ce) {
            if (ce->TestAttr(Attribute::COMPILER_ADD)) {
                PrintIndent(indent + ONE_INDENT, "super: implicitSuper");
            } else {
                PrintIndent(indent + ONE_INDENT, "super: explicitSuper");
            }
        }
    }
    PrintAnnotations(funcDecl, indent);
    PrintModifiers(funcDecl, indent);
    PrintNode(funcDecl.funcBody.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintMainDecl(unsigned indent, const MainDecl& mainDecl, std::string& tyStr)
{
    PrintIndent(indent, "MainDecl", mainDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (mainDecl.desugarDecl) {
        PrintIndent(indent + ONE_INDENT, "DesugarDecl: ");
        PrintNode(mainDecl.desugarDecl.get(), indent + ONE_INDENT);
    } else {
        PrintNode(mainDecl.funcBody.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintFuncBody(unsigned indent, const FuncBody& body, std::string& tyStr)
{
    PrintTy(indent + ONE_INDENT, tyStr);
    if (body.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : body.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : body.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    for (auto& it : body.paramLists) {
        PrintNode(it.get(), indent);
    }
    if (!body.retType) {
        PrintIndent(indent, "# no return type");
    } else {
        PrintIndent(indent, "# return type");
        PrintNode(body.retType.get(), indent + ONE_INDENT);
    }
    PrintNode(body.body.get(), indent, "FuncBody");
}

void PrintPropDecl(unsigned indent, const PropDecl& propDecl, std::string& tyStr)
{
    PrintIndent(indent, "PropDecl", propDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintModifiers(propDecl, indent);
    PrintNode(propDecl.type.get(), indent + ONE_INDENT, "type");
    for (auto& propMemberDecl : propDecl.setters) {
        PrintNode(propMemberDecl.get(), indent + ONE_INDENT);
    }
    for (auto& propMemberDecl : propDecl.getters) {
        PrintNode(propMemberDecl.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandDecl(unsigned indent, const AST::MacroExpandDecl& macroExpand)
{
    PrintIndent(indent, "MacroExpand:", macroExpand.invocation.fullName, "{");
    PrintMacroInvocation(indent, macroExpand.invocation);
    PrintIndent(indent, "}");
}

void PrintVarWithPatternDecl(unsigned indent, const VarWithPatternDecl& varWithPatternDecl, std::string& tyStr)
{
    PrintIndent(indent, "VarWithPatternDecl:", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (varWithPatternDecl.isConst) {
        PrintIndent(indent + ONE_INDENT, "Const");
    } else {
        PrintIndent(indent + ONE_INDENT, varWithPatternDecl.isVar ? "Var" : "Let");
    }
    PrintNode(varWithPatternDecl.irrefutablePattern.get(), indent + ONE_INDENT);
    PrintNode(varWithPatternDecl.type.get(), indent + ONE_INDENT, "type");
    PrintNode(varWithPatternDecl.initializer.get(), indent + ONE_INDENT, "initializer");
    PrintIndent(indent, "}");
}

void PrintVarDecl(unsigned indent, const VarDecl& varDecl, std::string& tyStr)
{
    PrintIndent(indent, "VarDecl", varDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintAnnotations(varDecl, indent);
    if (varDecl.isConst) {
        PrintIndent(indent + ONE_INDENT, "Const");
    } else {
        PrintModifiers(varDecl, indent);
        PrintIndent(indent + ONE_INDENT, varDecl.isVar ? "Var" : "Let");
    }
    PrintNode(varDecl.type.get(), indent + ONE_INDENT, "type");
    PrintNode(varDecl.initializer.get(), indent + ONE_INDENT, "initializer");
    PrintIndent(indent, "}");
}

void PrintTypeAliasDecl(unsigned indent, const TypeAliasDecl& alias, std::string& tyStr)
{
    PrintIndent(indent, "TypeAliasDecl", alias.identifier.Val(), "{");
    if (alias.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : alias.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : alias.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintNode(alias.type.get(), indent + ONE_INDENT, "target type");
    PrintTy(indent + ONE_INDENT, tyStr);
    std::string aliasStr = (!alias.ty) ? (ANSI_COLOR_RED + "unknown" + ANSI_COLOR_RESET) : alias.ty->String();
    PrintTy(indent + ONE_INDENT, "aliasedTy:" + aliasStr);
    PrintIndent(indent, "}");
}

void PrintClassDecl(unsigned indent, const ClassDecl& classDecl, std::string& tyStr)
{
    if (auto attr = HasJavaAttr(classDecl); attr) {
        std::string argStr = attr.value() == Attribute::JAVA_APP ? "app" : "ext";
        PrintIndent(indent, "@Java[\"" + argStr + "\"]");
    }
    PrintIndent(indent, "ClassDecl", classDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintAnnotations(classDecl, indent);
    PrintModifiers(classDecl, indent);
    if (classDecl.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : classDecl.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : classDecl.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    if (!classDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "# ClassOrInterfaceTypes", "{");
        for (auto& it : classDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "# ClassBody:", "{");
    PrintNode(classDecl.body.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintInterfaceDecl(unsigned indent, const InterfaceDecl& interfaceDecl, std::string& tyStr)
{
    if (auto attr = HasJavaAttr(interfaceDecl); attr) {
        PrintIndent(indent, "@Java[\"", attr.value() == Attribute::JAVA_APP ? "app" : "ext", "\"]");
    }
    PrintIndent(indent, "InterfaceDecl:", interfaceDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintModifiers(interfaceDecl, indent);
    if (interfaceDecl.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : interfaceDecl.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : interfaceDecl.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    if (!interfaceDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "# inheritedTypes:", "{");
        for (auto& it : interfaceDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "# InterfaceBody:", "{");
    PrintNode(interfaceDecl.body.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintEnumDecl(unsigned indent, const EnumDecl& enumDecl, std::string& tyStr)
{
    PrintIndent(indent, "EnumDecl", enumDecl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintModifiers(enumDecl, indent);
    if (enumDecl.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : enumDecl.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : enumDecl.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    if (!enumDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "# inheritedTypes", "{");
        for (auto& it : enumDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "constructors", "{");
    for (auto& it : enumDecl.constructors) {
        PrintIndent(indent + TWO_INDENT, ">caseBody", "{");
        PrintNode(it.get(), indent + THREE_INDENT);
        PrintIndent(indent + TWO_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent + ONE_INDENT, "functions", "{");
    for (auto& it : enumDecl.members) {
        PrintNode(it.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintStructDecl(unsigned indent, const StructDecl& decl, std::string& tyStr)
{
    if (decl.TestAttr(Attribute::C)) {
        PrintIndent(indent, "@C");
    }
    PrintIndent(indent, "StructDecl", decl.identifier.Val(), "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintAnnotations(decl, indent);
    PrintModifiers(decl, indent);
    if (decl.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : decl.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : decl.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintIndent(indent + ONE_INDENT, "# StructBody:", "{");
    PrintNode(decl.body.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintExtendDecl(unsigned indent, const ExtendDecl& ed)
{
    PrintIndent(indent, "ExtendDecl", "{");
    PrintIndent(indent + ONE_INDENT, "extendedType", ":");
    PrintNode(ed.extendedType.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "interfaces", "{");
    for (auto& interface : ed.inheritedTypes) {
        PrintNode(interface.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "}");
    if (ed.generic) {
        PrintIndent(indent, "typeParameters", ":");
        for (auto& it : ed.generic->typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "genericConstraints", ":");
        for (auto& it : ed.generic->genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintIndent(indent + ONE_INDENT, "extendMembers", "{");
    for (auto& it : ed.members) {
        PrintNode(it.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintIfExpr(unsigned indent, const IfExpr& expr, const std::string& tyStr, const std::string& str)
{
    if (expr.desugarExpr) {
        PrintNode(expr.desugarExpr.get(), indent, "MatchExpr desugared from IfExpr");
        return;
    }
    std::string str1 = (str != "ElseExpr") ? "# IfExpr" : "# ElseIfExpr";
    PrintIndent(indent, str1, expr.begin.ToString() + " {");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "condExpr", "{");
    PrintNode(expr.condExpr.get(), indent + TWO_INDENT, "condition expr");
    PrintIndent(indent + ONE_INDENT, "}");
    PrintNode(expr.thenBody.get(), indent + ONE_INDENT, "Control-Body begin=" + expr.thenBody->begin.ToString());
    if (expr.hasElse) {
        PrintNode(expr.elseBody.get(), indent + ONE_INDENT, "ElseExpr begin=" + expr.elseBody->ToString());
    }
    PrintIndent(indent, "}");
}

void PrintMatchCase(unsigned indent, const MatchCase& mc)
{
    PrintIndent(indent, "MatchCase:", "{");
    std::string tyStr = mc.ty ? mc.ty->String() : UNKNOWN_TY;
    PrintTy(indent + ONE_INDENT, tyStr);

    for (auto& it : mc.patterns) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "PatternGuard:");
    PrintNode(mc.patternGuard.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "CaseBody(ExprOrDecls):");
    for (auto& it : mc.exprOrDecls->body) {
        PrintNode(it.get(), indent + THREE_INDENT);
    }

    PrintIndent(indent, "}");
}

void PrintMatchExpr(unsigned indent, const MatchExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "MatchExpr:", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (expr.matchMode) {
        PrintIndent(indent + ONE_INDENT, "Selector:");
        PrintNode(expr.selector.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "MatchCases:");
        for (auto& matchCase : expr.matchCases) {
            PrintMatchCase(indent + TWO_INDENT, *StaticCast<MatchCase*>(matchCase.get()));
        }
    } else {
        for (auto& matchCaseOther : expr.matchCaseOthers) {
            PrintIndent(indent + ONE_INDENT, ">MatchCase:", "{");
            PrintNode(matchCaseOther->matchExpr.get(), indent + TWO_INDENT);
            PrintIndent(indent + TWO_INDENT, "PatternExprOrDecls:");
            for (auto& it : matchCaseOther->exprOrDecls->body) {
                PrintNode(it.get(), indent + THREE_INDENT);
            }
            PrintIndent(indent + ONE_INDENT, "}");
        }
    }
    PrintIndent(indent, "}");
}

void PrintTryExpr(unsigned indent, const TryExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "TryExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (!expr.resourceSpec.empty()) {
        PrintIndent(indent + ONE_INDENT, "ResourceSpec", "{");
        for (auto& i : expr.resourceSpec) {
            PrintNode(i.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "TryBlock", "{");
    PrintNode(expr.tryBlock.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    for (unsigned int i = 0; i < expr.catchBlocks.size(); i++) {
        PrintIndent(indent + ONE_INDENT, "Catch", "{");
        PrintIndent(indent + TWO_INDENT, "CatchPattern", "{");
        PrintNode(expr.catchPatterns[i].get(), indent + THREE_INDENT);
        PrintIndent(indent + TWO_INDENT, "}");
        PrintIndent(indent + TWO_INDENT, "CatchBlock", "{");
        PrintNode(expr.catchBlocks[i].get(), indent + THREE_INDENT);
        PrintIndent(indent + TWO_INDENT, "}");
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "FinallyBlock", "{");
    PrintNode(expr.finallyBlock.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintThrowExpr(unsigned indent, const ThrowExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "ThrowExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(expr.expr.get(), indent + ONE_INDENT, "ThrowExpr");
    PrintIndent(indent, "}");
}

void PrintReturnExpr(unsigned indent, const ReturnExpr& expr, std::string& tyStr)
{
    if (expr.desugarExpr) {
        PrintNode(expr.desugarExpr.get());
    } else {
        PrintIndent(indent, "ReturnExpr", "{");
        PrintTy(indent + ONE_INDENT, tyStr);
        PrintNode(expr.expr.get(), indent + ONE_INDENT, "ReturnExpr");
        PrintIndent(indent, "}");
    }
}

void PrintJumpExpr(unsigned indent, const JumpExpr& expr, std::string& tyStr)
{
    if (expr.desugarExpr) {
        PrintNode(expr.desugarExpr.get());
    } else {
        std::string str1 = expr.isBreak ? "Break" : "Continue";
        PrintIndent(indent, "JumpExpr:", str1);
        PrintTy(indent + ONE_INDENT, tyStr);
    }
}

void PrintForInExpr(unsigned indent, const ForInExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "# ForInExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "pattern", ":");
    PrintNode(expr.pattern.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "inExpression", ":");
    PrintNode(expr.inExpression.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "patternGuard", ":");
    PrintNode(expr.patternGuard.get(), indent + TWO_INDENT);
    PrintNode(expr.body.get(), indent + ONE_INDENT, "controlBody");
    PrintIndent(indent, "}");
}

void PrintWhileExpr(unsigned indent, const WhileExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "# WhileExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "condExpr", "{");
    PrintNode(expr.condExpr.get(), indent + TWO_INDENT, "condition expr");
    PrintIndent(indent + ONE_INDENT, "}");
    PrintNode(expr.body.get(), indent + ONE_INDENT, "While-Body");
    PrintIndent(indent, "}");
}

void PrintDoWhileExpr(unsigned indent, const DoWhileExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "# DoWhileExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(expr.body.get(), indent + ONE_INDENT, "DoWhile-Body");
    PrintIndent(indent + ONE_INDENT, "condExpr", "{");
    PrintNode(expr.condExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintAssignExpr(unsigned indent, const AssignExpr& expr, std::string& tyStr)
{
    const std::unordered_map<TokenKind, std::string> op2str = {
        {TokenKind::ASSIGN, "'='"},
        {TokenKind::ADD_ASSIGN, "'+='"},
        {TokenKind::SUB_ASSIGN, "'-='"},
        {TokenKind::MUL_ASSIGN, "'*='"},
        {TokenKind::EXP_ASSIGN, "'**='"},
        {TokenKind::DIV_ASSIGN, "'/='"},
        {TokenKind::MOD_ASSIGN, "'%%='"},
        {TokenKind::AND_ASSIGN, "'&&='"},
        {TokenKind::OR_ASSIGN, "'||='"},
        {TokenKind::BITAND_ASSIGN, "'&='"},
        {TokenKind::BITOR_ASSIGN, "'|='"},
        {TokenKind::BITXOR_ASSIGN, "'^='"},
        {TokenKind::LSHIFT_ASSIGN, "'<<='"},
        {TokenKind::RSHIFT_ASSIGN, "'>>='"},
    };
    PrintIndent(indent, "AssignExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    std::string str1;
    auto result = op2str.find(expr.op);
    if (result != op2str.end()) {
        str1 = result->second;
    }
    PrintIndent(indent + ONE_INDENT, str1);
    PrintNode(expr.leftValue.get(), indent + ONE_INDENT, "leftValue");
    PrintNode(expr.rightExpr.get(), indent + ONE_INDENT, "rightExpr");
    if (expr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(expr.overflowStrategy));
    }
    PrintIndent(indent, "}");
    if (expr.desugarExpr) {
        PrintIndent(indent, "# desugar expr of AssignExpr", "{");
        PrintNode(expr.desugarExpr.get(), indent + ONE_INDENT);
        PrintIndent(indent, "}");
    }
}

void PrintIncOrDecExpr(unsigned indent, const IncOrDecExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "IncOrDecExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    std::string str1 = (expr.op == TokenKind::INCR) ? "++" : "--";
    PrintIndent(indent + ONE_INDENT, str1);
    PrintNode(expr.expr.get(), indent + ONE_INDENT);
    if (expr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(expr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintUnaryExpr(unsigned indent, const UnaryExpr& unaryExpr, std::string& tyStr)
{
    PrintIndent(indent, "UnaryExpr", "{", TOKENS[static_cast<int>(unaryExpr.op)]);
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(unaryExpr.expr.get(), indent + ONE_INDENT);
    if (unaryExpr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(unaryExpr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintBinaryExpr(unsigned indent, const BinaryExpr& binaryExpr, std::string& tyStr)
{
    if (binaryExpr.desugarExpr) {
        return PrintNode(binaryExpr.desugarExpr.get(), indent, "desugared to");
    }
    PrintIndent(indent, "BinaryExpr " + binaryExpr.begin.ToString(), TOKENS[static_cast<int>(binaryExpr.op)], "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "left: " + binaryExpr.leftExpr->begin.ToString());
    PrintNode(binaryExpr.leftExpr.get(), indent + ONE_INDENT, binaryExpr.leftExpr->begin.ToString());
    PrintIndent(indent + ONE_INDENT, "right: " + binaryExpr.rightExpr->begin.ToString());
    PrintNode(binaryExpr.rightExpr.get(), indent + ONE_INDENT, binaryExpr.rightExpr->begin.ToString());
    if (binaryExpr.desugarExpr) {
        PrintIndent(indent + ONE_INDENT, "DesugarExpr: ");
        PrintNode(binaryExpr.desugarExpr.get(), indent + ONE_INDENT);
    }
    if (binaryExpr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(binaryExpr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintRangeExpr(unsigned indent, const RangeExpr& rangeExpr, std::string& tyStr)
{
    PrintIndent(indent, "RangeExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "Start:");
    PrintNode(rangeExpr.startExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "Stop:");
    PrintNode(rangeExpr.stopExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "Step:");
    PrintNode(rangeExpr.stepExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent, "}");
}

void PrintSubscriptExpr(unsigned indent, const SubscriptExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "SubscriptExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseExpr:");
    PrintNode(expr.baseExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "IndexExpr:");
    for (auto& it : expr.indexExprs) {
        PrintNode(it.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMemberAccess(unsigned indent, const MemberAccess& expr, std::string& tyStr)
{
    PrintIndent(indent, "MemberAccess", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseExpr:");
    PrintNode(expr.baseExpr.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "FieldIdentifier:");
    PrintIndent(indent + TWO_INDENT, expr.field.Val());
    if (expr.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no type arguments");
    } else {
        PrintIndent(indent + ONE_INDENT, "# type arguments");
        for (auto& it : expr.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
    }
    PrintIndent(indent, "}");
}

void PrintOptionalExpr(unsigned indent, const OptionalExpr& optionalExpr, std::string& tyStr)
{
    PrintIndent(indent, "OptionalExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseExpr:");
    PrintNode(optionalExpr.baseExpr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintOptionalChainExpr(unsigned indent, const OptionalChainExpr& optionalChainExpr, std::string& tyStr)
{
    PrintIndent(indent, "OptionalChainExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "Expr:");
    PrintNode(optionalChainExpr.expr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintLetPatternDestructor(unsigned indent, const LetPatternDestructor& expr, const std::string& tyStr)
{
    if (expr.desugarExpr) {
        return PrintNode(expr.desugarExpr.get(), indent, "desugared to");
    }
    PrintIndent(indent, "LetPattern", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, ">LetPatterns:", "{");
    for (auto& p : expr.patterns) {
        PrintIndent(indent + TWO_INDENT, "|", "{");
        PrintNode(p.get(), indent + THREE_INDENT);
        PrintIndent(indent + TWO_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent + ONE_INDENT, "Initializer:", "{");
    PrintNode(expr.initializer.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintFuncArg(unsigned indent, const FuncArg& expr, std::string& tyStr)
{
    PrintIndent(indent, "FuncArg", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (!expr.name.Empty()) {
        PrintIndent(indent + ONE_INDENT, "ArgName:", expr.name.Val());
    }
    if (expr.withInout) {
        PrintIndent(indent + ONE_INDENT, "inout");
    }
    PrintNode(expr.expr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintCallExpr(unsigned indent, const CallExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "CallExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseFunc:");
    PrintNode(expr.baseFunc.get(), indent + TWO_INDENT);
    if (expr.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no arguments");
    } else {
        PrintIndent(indent + ONE_INDENT, "# arguments");
        for (auto& it : expr.args) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
    }
    PrintIndent(indent, "}");
    if (expr.desugarExpr != nullptr) {
        PrintIndent(indent, "# desugar expr ", "{");
        PrintNode(expr.desugarExpr.get(), indent + ONE_INDENT);
        PrintIndent(indent, "}");
    }
}

void PrintParenExpr(unsigned indent, const ParenExpr& parenExpr, std::string& tyStr)
{
    PrintIndent(indent, "ParenExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(parenExpr.expr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintLambdaExpr(unsigned indent, const LambdaExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "LambdaExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(expr.funcBody.get(), indent + ONE_INDENT, "LambdaExpr-Body");
    PrintIndent(indent, "}");
}

void PrintLitConstExpr(unsigned indent, const LitConstExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "LitConstExpr:", expr.LitKindToString(), StringConvertor::Recover(expr.stringValue));
    PrintTy(indent + ONE_INDENT, tyStr);
    if (expr.desugarExpr != nullptr) {
        PrintIndent(indent + ONE_INDENT, "# desugar expr");
        PrintNode(expr.desugarExpr.get(), indent + TWO_INDENT);
    }
}

void PrintStrInterpolationExpr(unsigned indent, const StrInterpolationExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "StrInterpolationExpr:", expr.rawString);
    PrintTy(indent + ONE_INDENT, tyStr);
    if (expr.desugarExpr != nullptr) {
        PrintIndent(indent + ONE_INDENT, "# desugar expr");
        PrintNode(expr.desugarExpr.get(), indent + TWO_INDENT);
    }
}

void PrintArrayLit(unsigned indent, const ArrayLit& expr, std::string& tyStr)
{
    PrintIndent(indent, "ArrayLiteral", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (expr.children.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no members");
    }
    for (auto& it : expr.children) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintArrayExpr(unsigned indent, const ArrayExpr& expr, std::string& tyStr)
{
    auto typeName = expr.isValueArray ? "ValueArrayExpr" : "ArrayExpr";
    PrintIndent(indent, typeName, "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    for (size_t i = 0; i < expr.args.size(); ++i) {
        PrintIndent(indent + ONE_INDENT, "Arg", std::to_string(i), ":");
        PrintNode(expr.args[i].get(), indent + TWO_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintTupleLit(unsigned indent, const TupleLit& expr, std::string& tyStr)
{
    PrintIndent(indent, "TupleLiteral", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    for (auto& it : expr.children) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintTypeConvExpr(unsigned indent, const TypeConvExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "TypeConvExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "# Convert Type:");
    PrintNode(expr.type.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "# Expr:");
    PrintNode(expr.expr.get(), indent + TWO_INDENT);
    PrintIndent(indent, "}");
}

void PrintRefExpr(unsigned indent, const RefExpr& refExpr, std::string& tyStr)
{
    PrintIndent(indent, "RefExpr :", refExpr.ref.identifier.Val());
    PrintTy(indent + ONE_INDENT, tyStr);
    if (refExpr.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no type arguments");
    } else {
        PrintIndent(indent + ONE_INDENT, "# type arguments");
        for (auto& it : refExpr.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
    }
    if (refExpr.desugarExpr != nullptr) {
        PrintIndent(indent + ONE_INDENT, "# desugar expr");
        PrintNode(refExpr.desugarExpr.get(), indent + TWO_INDENT);
    }
}

void PrintSpawnExpr(unsigned indent, const SpawnExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "# SpawnExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (expr.futureObj) {
        PrintNode(expr.futureObj.get(), indent + ONE_INDENT);
    }
    if (expr.arg) {
        PrintNode(expr.arg.get(), indent + ONE_INDENT);
    }
    if (expr.task) {
        PrintNode(expr.task.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintSynchronizedExpr(unsigned indent, const SynchronizedExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "# SynchronizedExpr", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "# Mutex:");
    PrintNode(expr.mutex.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "# Synchronized body:");
    PrintNode(expr.body.get(), indent + TWO_INDENT);
    PrintIndent(indent, "}");
}

void PrintBlock(unsigned indent, const Block& block, const std::string& tyStr, const std::string& str)
{
    PrintIndent(indent, "#", str, "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    if (block.body.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no block");
    } else {
        for (auto& it : block.body) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintIndent(indent, "}");
}

void PrintInterfaceBody(unsigned indent, const InterfaceBody& body)
{
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent);
    }
}

void PrintClassBody(unsigned indent, const ClassBody& body)
{
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent);
    }
}

void PrintStructBody(unsigned indent, const StructBody& body)
{
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent);
    }
}

void PrintIsExpr(unsigned indent, const IsExpr& isExpr, std::string& tyStr)
{
    PrintIndent(indent, "IsExpr : ", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent, "LeftExpr :");
    if (isExpr.desugarExpr && isExpr.desugarExpr->astKind == ASTKind::MATCH_EXPR) {
        auto me = StaticCast<MatchExpr*>(isExpr.desugarExpr.get());
        PrintNode(me->selector.get(), indent + ONE_INDENT);
    } else {
        PrintNode(isExpr.leftExpr.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "IsType :");
    PrintNode(isExpr.isType.get(), indent + ONE_INDENT);
}

void PrintAsExpr(unsigned indent, const AsExpr& asExpr, std::string& tyStr)
{
    PrintIndent(indent, "AsExpr : ", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent, "LeftExpr :");
    if (asExpr.desugarExpr && asExpr.desugarExpr->astKind == ASTKind::MATCH_EXPR) {
        auto me = StaticCast<MatchExpr*>(asExpr.desugarExpr.get());
        PrintNode(me->selector.get(), indent + ONE_INDENT);
    } else {
        PrintNode(asExpr.leftExpr.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "AsType :");
    PrintNode(asExpr.asType.get(), indent + ONE_INDENT);
}

void PrintTokenPart(unsigned indent, const AST::TokenPart& tokenPart)
{
    PrintIndentTokens(indent + ONE_INDENT, tokenPart.tokens);
}

void PrintQuoteExpr(unsigned indent, const AST::QuoteExpr& qe)
{
    PrintIndent(indent, "QuoteExpr", "{");
    PrintIndent(indent + ONE_INDENT, "content:", qe.content);
    for (auto& expr : qe.exprs) {
        if (expr->astKind == ASTKind::TOKEN_PART) {
            PrintIndent(indent + ONE_INDENT, "TokenPart", "(");
        } else {
            PrintIndent(indent + ONE_INDENT, "DollarExpr", "(");
        }
        PrintNode(expr.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, ")");
    }
    if (qe.desugarExpr) {
        PrintIndent(indent + ONE_INDENT, "# desugar expr");
        PrintNode(qe.desugarExpr.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandExpr(unsigned indent, const AST::MacroExpandExpr& expr, std::string& tyStr)
{
    PrintIndent(indent, "MacroExpand:", expr.invocation.fullName, "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintMacroInvocation(indent, expr.invocation);
    PrintIndent(indent, "}");
}

void PrintRefType(unsigned indent, const RefType& refType, std::string& tyStr)
{
    PrintIndent(indent, "RefType:", refType.ref.identifier.Val());
    PrintTy(indent + ONE_INDENT, tyStr);
    if (refType.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "# no type arguments");
    } else {
        PrintIndent(indent + ONE_INDENT, "# type arguments");
        for (auto& it : refType.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
    }
}

void PrintParenType(unsigned indent, const ParenType& type, std::string& tyStr)
{
    PrintIndent(indent, "ParenType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintNode(type.type.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintFuncType(unsigned indent, const FuncType& type, std::string& tyStr)
{
    PrintIndent(indent, "FuncType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "FuncParamType:");
    for (auto& paramType : type.paramTypes) {
        PrintNode(paramType.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "FuncRetType:");
    PrintNode(type.retType.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "IsCFuncType:");
    PrintIndent(indent + TWO_INDENT, type.isC);
    PrintIndent(indent, "}");
}

void PrintTupleType(unsigned indent, const TupleType& type, std::string& tyStr)
{
    PrintIndent(indent, "TupleType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "TupleTypeList:");
    for (auto& it : type.fieldTypes) {
        PrintNode(it.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintThisType(unsigned indent, std::string& tyStr)
{
    PrintIndent(indent, "ThisType");
    PrintTy(indent + ONE_INDENT, tyStr);
}

void PrintPrimitiveType(unsigned indent, const PrimitiveType& type, std::string& tyStr)
{
    PrintIndent(indent, "PrimitiveType:", type.str);
    PrintTy(indent + ONE_INDENT, tyStr);
}

void PrintOptionType(unsigned indent, const OptionType& type, std::string& tyStr)
{
    PrintIndent(indent, "OptionType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseType:");
    PrintNode(type.componentType.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "QuestNums:", type.questNum);
    PrintIndent(indent, "}");
}

void PrintVArrayType(unsigned indent, const VArrayType& type, std::string& tyStr)
{
    PrintIndent(indent, "VArrayType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "typeArguments:");
    PrintNode(type.typeArgument.get(), indent + TWO_INDENT);
    PrintIndent(indent, "}");
}

void PrintQualifiedType(unsigned indent, const QualifiedType& type, std::string& tyStr)
{
    PrintIndent(indent, "QualifiedType", "{");
    PrintTy(indent + ONE_INDENT, tyStr);
    PrintIndent(indent + ONE_INDENT, "BaseType:");
    PrintNode(type.baseType.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "FieldIdentifier:");
    PrintIndent(indent + TWO_INDENT, type.field.Val());
    PrintIndent(indent, "}");
}

void PrintGenericConstraint(unsigned indent, const GenericConstraint& generic)
{
    PrintIndent(indent, "GenericConstraint {");
    PrintNode(generic.type.get(), indent + ONE_INDENT);
    for (auto& upperBound : generic.upperBounds) {
        PrintNode(upperBound.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintConstPattern(unsigned indent, const ConstPattern& constPattern)
{
    PrintIndent(indent, "ConstPattern:");
    PrintNode(constPattern.literal.get(), indent + ONE_INDENT);
}

void PrintTuplePattern(unsigned indent, const TuplePattern& tuplePattern)
{
    PrintIndent(indent, "TuplePattern:");
    for (auto& pattern : tuplePattern.patterns) {
        PrintNode(pattern.get(), indent + ONE_INDENT);
    }
}

void PrintTypePattern(unsigned indent, const TypePattern& typePattern)
{
    PrintIndent(indent, "TypePattern:");
    PrintNode(typePattern.pattern.get(), indent + ONE_INDENT);
    PrintNode(typePattern.type.get(), indent + ONE_INDENT);
    if (typePattern.desugarExpr.get()) {
        PrintIndent(indent, "# autobox typePattern");
        PrintNode(typePattern.desugarExpr.get(), indent + ONE_INDENT);
    }
    if (typePattern.desugarVarPattern.get()) {
        PrintIndent(indent, "# unboxing typePattern");
        PrintNode(typePattern.desugarVarPattern.get(), indent + ONE_INDENT);
    }
}

void PrintVarPattern(unsigned indent, const VarPattern& varPattern)
{
    PrintIndent(indent, "VariablePattern:", varPattern.varDecl->identifier.Val());
    std::string tyStr = varPattern.ty ? varPattern.ty->String() : UNKNOWN_TY;
    PrintTy(indent + ONE_INDENT, tyStr);
    if (varPattern.desugarExpr.get()) {
        PrintIndent(indent, "# DesugarExpr");
        PrintNode(varPattern.desugarExpr.get(), indent + ONE_INDENT);
    }
}

void PrintEnumPattern(unsigned indent, const EnumPattern& enumPattern)
{
    PrintIndent(indent, "EnumPattern:");
    PrintNode(enumPattern.constructor.get(), indent + ONE_INDENT);
    for (auto& pattern : enumPattern.patterns) {
        PrintNode(pattern.get(), indent + ONE_INDENT);
    }
}

void PrintExceptTypePattern(unsigned indent, const ExceptTypePattern& exceptTypePattern)
{
    PrintIndent(indent, "ExceptTypePattern:");
    PrintNode(exceptTypePattern.pattern.get(), indent + ONE_INDENT);
    for (auto& type : exceptTypePattern.types) {
        PrintNode(type.get(), indent + ONE_INDENT);
    }
}

void PrintVarOrEnumPattern(unsigned indent, const VarOrEnumPattern& ve)
{
    PrintIndent(indent, "VarOrEnumPattern {");
    PrintIndent(indent + ONE_INDENT, ve.identifier);
    if (ve.pattern) {
        PrintIndent(indent + ONE_INDENT, "Subpatterns {");
        PrintNode(ve.pattern, indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent, "}");
}

void PrintPackageSpec(unsigned indent, const PackageSpec& package)
{
    PrintIndent(indent, "PackageHeader:", package.packageName.Val());
    if (!package.prefixPaths.empty()) {
        auto prefixPath = JoinStrings(package.prefixPaths, ".");
        PrintIndent(indent + ONE_INDENT, ">PrefixPath:", std::move(prefixPath));
    }
    if (package.modifier) {
        PrintIndent(indent + ONE_INDENT, ">Modifier:", TOKENS[static_cast<int>(package.modifier->modifier)]);
    }
}

void PrintImportSpec(unsigned indent, const ImportSpec& import)
{
    if (import.content.kind != ImportKind::IMPORT_MULTI) {
        PrintIndent(indent, "ImportPackage:", import.content.identifier.Val());
        if (import.content.kind == ImportKind::IMPORT_ALIAS) {
            PrintIndent(indent + ONE_INDENT, ">AliasName:", import.content.aliasName.Val());
        }
        if (!import.content.prefixPaths.empty()) {
            PrintIndent(indent + ONE_INDENT, ">PrefixPath:", JoinStrings(import.content.prefixPaths, "."));
        }
        if (import.modifier) {
            PrintIndent(indent + ONE_INDENT, ">Modifier:", TOKENS[static_cast<int>(import.modifier->modifier)]);
        }
    } else {
        std::string commonPrefix = JoinStrings(import.content.prefixPaths, ".");
        for (const auto& item : import.content.items) {
            PrintIndent(indent, "ImportPackage:", item.identifier.Val());
            if (item.kind == ImportKind::IMPORT_ALIAS) {
                PrintIndent(indent + ONE_INDENT, ">AliasName:", item.aliasName.Val());
            }
            std::stringstream ss;
            ss << commonPrefix;
            if (!commonPrefix.empty() && !item.prefixPaths.empty()) {
                ss << "." << JoinStrings(item.prefixPaths, ".");
            } else if (!item.prefixPaths.empty()) {
                ss << JoinStrings(item.prefixPaths, ".");
            }
            if (ss.rdbuf()->in_avail() != 0) {
                PrintIndent(indent + ONE_INDENT, ">PrefixPath:", ss.str());
            }
            if (import.modifier) {
                PrintIndent(indent + ONE_INDENT, ">Modifier:", TOKENS[static_cast<int>(import.modifier->modifier)]);
            }
        }
    }
}
} // namespace
#endif

void PrintNode(
    [[maybe_unused]] Ptr<const Node> node, [[maybe_unused]] unsigned indent, [[maybe_unused]] const std::string& str)
{
#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT) // Enable printNode for debug and CI version.
    if (!node) {
        return PrintIndent(indent, "# no " + str);
    }

    if (node->IsExpr()) {
        auto expr = RawStaticCast<const Expr*>(node);
        if (expr->desugarExpr) {
            return PrintNode(expr->desugarExpr.get(), indent, str);
        }
    }

    std::string tyStr = (!node->ty) ? (ANSI_COLOR_RED + "unknown" + ANSI_COLOR_RESET) : node->ty->String();
    match (*node)([&indent](const Package& package) { PrintPackage(indent, package); },
        [&indent](const File& file) { PrintFile(indent, file); },
        // ----------- Decls --------------------
        [&indent](
            const GenericParamDecl& typeDecl) { PrintIndent(indent, "GenericParamDecl", typeDecl.identifier.Val()); },
        [&indent, &tyStr](const FuncParam& param) { PrintFuncParam(indent, param, tyStr); },
        [&indent](const MacroExpandParam& macroExpand) { PrintMacroExpandParam(indent, macroExpand); },
        [&indent](const FuncParamList& paramList) { PrintFuncParamList(indent, paramList); },
        [&indent, &tyStr](const MainDecl& mainDecl) { PrintMainDecl(indent, mainDecl, tyStr); },
        [&indent, &tyStr](const FuncDecl& funcDecl) { PrintFuncDecl(indent, funcDecl, tyStr); },
        [&indent, &tyStr](const MacroDecl& macroDecl) { PrintMacroDecl(indent, macroDecl, tyStr); },
        [&indent, &tyStr](const FuncBody& body) { PrintFuncBody(indent, body, tyStr); },
        [&indent, &tyStr](const PropDecl& propDecl) { PrintPropDecl(indent, propDecl, tyStr); },
        [&indent](const MacroExpandDecl& macroExpand) { PrintMacroExpandDecl(indent, macroExpand); },
        [&indent, &tyStr](const VarWithPatternDecl& varWithPatternDecl) {
            PrintVarWithPatternDecl(indent, varWithPatternDecl, tyStr);
        },
        [&indent, &tyStr](const VarDecl& varDecl) { PrintVarDecl(indent, varDecl, tyStr); },
        [&indent, &tyStr](const TypeAliasDecl& alias) { PrintTypeAliasDecl(indent, alias, tyStr); },
        [&indent, &tyStr](const ClassDecl& classDecl) { PrintClassDecl(indent, classDecl, tyStr); },
        [&indent, &tyStr](const InterfaceDecl& interfaceDecl) { PrintInterfaceDecl(indent, interfaceDecl, tyStr); },
        [&indent, &tyStr](const EnumDecl& enumDecl) { PrintEnumDecl(indent, enumDecl, tyStr); },
        [&indent, &tyStr](const StructDecl& decl) { PrintStructDecl(indent, decl, tyStr); },
        [&indent](const ExtendDecl& ed) { PrintExtendDecl(indent, ed); }, [](const PrimaryCtorDecl&) {},
        // -----------Exprs----------------------
        [&indent, &tyStr, &str](const IfExpr& expr) { PrintIfExpr(indent, expr, tyStr, str); },
        [&indent, &tyStr](const MatchExpr& expr) { PrintMatchExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const TryExpr& expr) { PrintTryExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const ThrowExpr& expr) { PrintThrowExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const ReturnExpr& expr) { PrintReturnExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const JumpExpr& expr) { PrintJumpExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const ForInExpr& expr) { PrintForInExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const WhileExpr& expr) { PrintWhileExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const DoWhileExpr& expr) { PrintDoWhileExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const AssignExpr& expr) { PrintAssignExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const IncOrDecExpr& expr) { PrintIncOrDecExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const UnaryExpr& unaryExpr) { PrintUnaryExpr(indent, unaryExpr, tyStr); },
        [&indent, &tyStr](const BinaryExpr& binaryExpr) { PrintBinaryExpr(indent, binaryExpr, tyStr); },
        [&indent, &tyStr](const RangeExpr& rangeExpr) { PrintRangeExpr(indent, rangeExpr, tyStr); },
        [&indent, &tyStr](const SubscriptExpr& expr) { PrintSubscriptExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const MemberAccess& expr) { PrintMemberAccess(indent, expr, tyStr); },
        [&indent, &tyStr](const FuncArg& expr) { PrintFuncArg(indent, expr, tyStr); },
        [&indent, &tyStr](const CallExpr& expr) { PrintCallExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const ParenExpr& parenExpr) { PrintParenExpr(indent, parenExpr, tyStr); },
        [&indent, &tyStr](const LambdaExpr& expr) { PrintLambdaExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const LitConstExpr& expr) { PrintLitConstExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const StrInterpolationExpr& expr) { PrintStrInterpolationExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const ArrayLit& expr) { PrintArrayLit(indent, expr, tyStr); },
        [&indent, &tyStr](const ArrayExpr& expr) { PrintArrayExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const TupleLit& expr) { PrintTupleLit(indent, expr, tyStr); },
        [&indent, &tyStr](const TypeConvExpr& expr) { PrintTypeConvExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const RefExpr& refExpr) { PrintRefExpr(indent, refExpr, tyStr); },
        [&indent, &tyStr](const OptionalExpr& expr) { PrintOptionalExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const OptionalChainExpr& expr) { PrintOptionalChainExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const LetPatternDestructor& expr) { PrintLetPatternDestructor(indent, expr, tyStr); },
        [&indent, &tyStr](
            const PrimitiveTypeExpr& /* primitiveTypeExpr */) { PrintIndent(indent, "PrimitiveTypeExpr: " + tyStr); },
        [&indent, &tyStr](const SpawnExpr& expr) { PrintSpawnExpr(indent, expr, tyStr); },
        [&indent, &tyStr](const SynchronizedExpr& expr) { PrintSynchronizedExpr(indent, expr, tyStr); },
        [](const InvalidExpr& /* expr */) { PrintIndent(0, "InvalidExpr: Need to be fixed!"); },
        [&indent, &str, &tyStr](const Block& block) { PrintBlock(indent, block, tyStr, str); },
        [&indent](const InterfaceBody& body) { PrintInterfaceBody(indent, body); },
        [&indent](const ClassBody& body) { PrintClassBody(indent, body); },
        [&indent](const StructBody& body) { PrintStructBody(indent, body); },
        [&indent, &tyStr](const IsExpr& isExpr) { PrintIsExpr(indent, isExpr, tyStr); },
        [&indent, &tyStr](const AsExpr& asExpr) { PrintAsExpr(indent, asExpr, tyStr); },
        [&indent](const TokenPart& tokenPart) { PrintTokenPart(indent, tokenPart); },
        [&indent](const QuoteExpr& quoteExpr) { PrintQuoteExpr(indent, quoteExpr); },
        [&indent, &tyStr](const MacroExpandExpr& macroExpr) { PrintMacroExpandExpr(indent, macroExpr, tyStr); },
        [&indent](const WildcardExpr& /* WildcardExpr */) { PrintIndent(indent, "WildcardExpr:", "_"); },
        // ----------- Type --------------------
        [&indent, &tyStr](const RefType& refType) { PrintRefType(indent, refType, tyStr); },
        [&indent, &tyStr](const ParenType& type) { PrintParenType(indent, type, tyStr); },
        [&indent, &tyStr](const FuncType& type) { PrintFuncType(indent, type, tyStr); },
        [&indent, &tyStr](const TupleType& type) { PrintTupleType(indent, type, tyStr); },
        [&indent, &tyStr](const ThisType& /* type */) { PrintThisType(indent, tyStr); },
        [&indent, &tyStr](const PrimitiveType& type) { PrintPrimitiveType(indent, type, tyStr); },
        [&indent, &tyStr](const OptionType& type) { PrintOptionType(indent, type, tyStr); },
        [&indent, &tyStr](const VArrayType& type) { PrintVArrayType(indent, type, tyStr); },
        [&indent, &tyStr](const QualifiedType& type) { PrintQualifiedType(indent, type, tyStr); },
        [&indent](const GenericConstraint& generic) { PrintGenericConstraint(indent, generic); },
        [](const InvalidType&) { PrintIndent(0, "InvalidType: Need to be fixed!"); },
        // -----------pattern----------------------
        [&indent](const ConstPattern& constPattern) { PrintConstPattern(indent, constPattern); },
        [&indent](const WildcardPattern& /* WildcardPattern */) { PrintIndent(indent, "WildcardPattern:", "_"); },
        [&indent](const VarPattern& varPattern) { PrintVarPattern(indent, varPattern); },
        [&indent](const TuplePattern& tuplePattern) { PrintTuplePattern(indent, tuplePattern); },
        [&indent](const TypePattern& typePattern) { PrintTypePattern(indent, typePattern); },
        [&indent](const EnumPattern& enumPattern) { PrintEnumPattern(indent, enumPattern); },
        [&indent](const ExceptTypePattern& exceptTypePattern) { PrintExceptTypePattern(indent, exceptTypePattern); },
        [indent](const VarOrEnumPattern& ve) { PrintVarOrEnumPattern(indent, ve); },
        // ----------- package----------------------
        [&indent](const PackageSpec& package) { PrintPackageSpec(indent, package); },
        [&indent](const ImportSpec& import) {
            if (import.IsImportMulti()) {
                return;
            }
            PrintImportSpec(indent, import);
        },
        // -----------no match----------------------
        [&node]() {
            const auto n = *node.get();
            std::cout << typeid(n).name() << " " << std::endl;
        });
#endif
}
} // namespace Cangjie
