// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file implements tostring utils API for CHIR
 */

#include "cangjie/CHIR/ToStringUtils.h"

#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/Expression.h"
#include "cangjie/CHIR/Type/ClassDef.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/Utils/Utils.h"

namespace Cangjie::CHIR {
void PrintIndent(std::ostream& stream, unsigned indent)
{
    for (unsigned i = 0; i < indent * 2; i++) { // 2 denote width of indent.
        stream << " ";
    }
}

std::string GetGenericConstaintsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::stringstream ss;
    ss << "[";
    size_t i = 0;
    for (auto genericTypeParam : genericTypeParams) {
        auto upperBounds = genericTypeParam->GetUpperBounds();
        if (upperBounds.empty()) {
            ++i;
            continue;
        }
        ss << genericTypeParam->ToString();
        ss << " <: ";
        size_t j = 0;
        for (auto upperBound : upperBounds) {
            ss << upperBound->ToString();
            if (j < upperBounds.size() - 1) {
                ss << " & ";
            }
            ++j;
        }
        if (i < genericTypeParams.size() - 1) {
            ss << ", ";
        }
        ++i;
    }
    ss << "]";

    // Skip the "[]" if there is no constraints
    constexpr int bracketsSize{2};
    if (ss.str().size() == bracketsSize) {
        return "";
    }
    return ss.str();
}

std::string GetBlockStr(const Block& block, unsigned indent)
{
    std::stringstream ss;
    PrintIndent(ss, indent);
    ss << block.GetAttributeInfo().ToString();
    ss << "Block " << block.GetIdentifier() << ": ";
    ss << "// preds: ";
    auto predecessors = block.GetPredecessors();
    for (size_t i = 0; i < predecessors.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << "#" << predecessors[i]->GetIdentifierWithoutPrefix();
    }
    if (auto annostr = block.ToStringAnnotationMap(); !annostr.empty()) {
        ss << " // " << annostr;
    }
    if (block.IsLandingPadBlock()) {
        PrintIndent(ss, indent + 1);
        ss << "// exceptions: " << GetExceptionsStr(block.GetExceptions());
    }
    ss << std::endl;
    auto exprs = block.GetExpressions();
    for (auto& expr : exprs) {
        PrintIndent(ss, indent + 1);
        if (auto res = expr->GetResult(); res != nullptr) {
            if (expr->GetResult()->IsRetValue()) {
                ss << "[ret] ";
            }
            ss << res->GetAttributeInfo().ToString();
            ss << res->GetIdentifier() << ": " << res->GetType()->ToString() << " = ";
        }
        ss << expr->ToString(indent + 1);
        if (auto annostr = expr->ToStringAnnotationMap(); !annostr.empty()) {
            ss << " // " << annostr;
        }
        ss << "\n";
    }
    return ss.str();
}

std::string GetBlockGroupStr(const BlockGroup& blockGroup, unsigned indent)
{
    std::stringstream ss;
    PrintIndent(ss, indent);
    ss << "{ "
       << "// Block Group: " << blockGroup.GetIdentifier() << "\n";

    if (!blockGroup.GetBlocks().empty()) {
        auto cmp = [](const Ptr<const Block> b1, const Ptr<const Block> b2) {
            return b1->GetIdentifier() < b2->GetIdentifier();
        };
        auto blocks = Utils::VecToSortedSet<decltype(cmp)>(blockGroup.GetBlocks(), cmp);
        auto sortedBlock = TopologicalSort(blockGroup.GetEntryBlock());
        for (auto block : sortedBlock) {
            ss << GetBlockStr(*block, indent);
            blocks.erase(block);
        }

        // print orphan block
        for (auto block : blocks) {
            ss << GetBlockStr(*block, indent);
        }
    }

    PrintIndent(ss, indent);
    ss << "}";
    return ss.str();
}

std::string GetGenericTypeParamsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::string res;
    if (!genericTypeParams.empty()) {
        res += "<";
        size_t i = 0;
        for (auto genericTypeParam : genericTypeParams) {
            res += genericTypeParam->ToString();
            if (i < genericTypeParams.size() - 1) {
                res += ", ";
            }
            ++i;
        }
        res += ">";
    }

    return res;
}

std::string GetGenericTypeParamsConstraintsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::string res;
    if (!genericTypeParams.empty()) {
        auto constraintsStr = GetGenericConstaintsStr(genericTypeParams);
        if (!constraintsStr.empty()) {
            res += "genericConstraints: " + constraintsStr;
        }
    }

    return res;
}

std::string GetLambdaStr(const Lambda& lambda, unsigned indent)
{
    std::stringstream ss;
    if (lambda.IsCompileTimeValue()) {
        ss << "[compileTimeVal] ";
    }
    if (lambda.IsLocalFunc()) {
        ss << "[localFunc] ";
    }
    ss << "Lambda";
    if (!lambda.GetIdentifier().empty()) {
        ss << "[" << lambda.GetIdentifier() << "]";
    }
    ss << GetGenericTypeParamsStr(lambda.GetGenericTypeParams());
    ss << "(";
    auto& params = lambda.GetParams();
    if (!params.empty()) {
        ss << params[0]->GetAttributeInfo().ToString();
        ss << params[0]->GetIdentifier() << ": " << params[0]->GetType()->ToString();
    }
    for (size_t i = 1; i < params.size(); i++) {
        ss << ", ";
        ss << params[i]->GetAttributeInfo().ToString();
        ss << params[i]->GetIdentifier() << ": " << params[i]->GetType()->ToString();
    }
    ss << ")";
    ss << "=> {";
    ss << " // ";
    ss << " srcCodeIdentifier: " << lambda.GetSrcCodeIdentifier();
    // captured vars
    if (auto capturedVars = lambda.GetCapturedVars(); !capturedVars.empty()) {
        ss << " capturedVars: ";
        for (size_t i = 0; i < capturedVars.size(); ++i) {
            ss << capturedVars[i]->GetIdentifier();
            if (i != capturedVars.size() - 1) {
                ss << ", ";
            }
        }
    }
    ss << GetGenericTypeParamsConstraintsStr(lambda.GetGenericTypeParams());
    ss << "\n" << GetBlockGroupStr(*lambda.GetBody(), indent + 1);
    ss << "}";
    return ss.str();
}

std::string GetFuncStr(const Func& func, unsigned indent)
{
    std::stringstream ss;
    ss << func.GetAttributeInfo().ToString();
    if (func.IsFastNative()) {
        ss << "[fastNative] ";
    }
    if (func.IsCFFIWrapper()) {
        ss << "[CFFIWrapper] ";
    }
    ss << "Func " << func.GetIdentifier();
    ss << GetGenericTypeParamsStr(func.GetGenericTypeParams());
    ss << "(";
    auto& params = func.GetParams();
    if (!params.empty()) {
        ss << params[0]->GetAttributeInfo().ToString();
        ss << params[0]->GetIdentifier() << ": " << params[0]->GetType()->ToString();
    }
    for (size_t i = 1; i < params.size(); i++) {
        ss << ", ";
        ss << params[i]->GetAttributeInfo().ToString();
        ss << params[i]->GetIdentifier() << ": " << params[i]->GetType()->ToString();
    }
    ss << ") : " << func.GetReturnType()->ToString();
    std::stringstream attrss;
    attrss << GetGenericTypeParamsConstraintsStr(func.GetGenericTypeParams());
    if (auto kind = func.GetFuncKind(); kind != FuncKind::DEFAULT) {
        if (attrss.str() != "") {
            attrss << ", ";
        }
        attrss << "kind: " << FUNCKIND_TO_STRING.at(kind);
    }
    auto annostr = func.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (func.GetGenericDecl() != nullptr) {
        attrss << ", genericDecl: " << func.GetGenericDecl()->GetIdentifierWithoutPrefix();
    }
    if (func.GetParentCustomTypeDef() != nullptr) {
        attrss << ", declared parent: " << func.GetParentCustomTypeDef()->GetIdentifierWithoutPrefix();
    }
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    if (!func.GetParentRawMangledName().empty()) {
        ss << " extendParentName: " << func.GetParentRawMangledName();
    }
    auto funcAnno = func.GetAnnoInfo();
    if (funcAnno.IsAvailable()) {
        ss << " funcAnnoInfo: " << funcAnno.mangledName;
    }
    if (!func.GetSrcCodeIdentifier().empty()) {
        ss << " srcCodeIdentifier: " << func.GetSrcCodeIdentifier();
    }
    std::vector<Parameter*> paramWithAnnos;
    paramWithAnnos.reserve(params.size());
    std::copy_if(params.begin(), params.end(), std::back_inserter(paramWithAnnos),
        [](const Ptr<const Parameter>& param) { return param->GetAnnoInfo().IsAvailable(); });
    if (!paramWithAnnos.empty()) {
        ss << " paramAnnoInfo: ";
        for (size_t i = 0; i < paramWithAnnos.size(); ++i) {
            if (i != paramWithAnnos.size() - 1) {
                ss << ", ";
            }
            auto paramAnno = paramWithAnnos[i]->GetAnnoInfo();
            ss << paramWithAnnos[i]->GetSrcCodeIdentifier() + " : " + paramAnno.mangledName;
        }
    }
    ss << "\n" << GetBlockGroupStr(*func.GetBody(), indent);
    return ss.str();
}

std::string GetImportedValueStr(const ImportedValue& value)
{
    if (auto f = DynamicCast<ImportedFunc>(&value)) {
        return GetImportedFuncStr(*f);
    }
    std::stringstream ss;
    ss << "#from <" << value.GetSourcePackageName() << "> ";
    ss << "import " << value.GetAttributeInfo().ToString() << value.GetIdentifier() << ": "
       << value.GetType()->ToString();
    std::stringstream attrss;
    auto annostr = value.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    return ss.str();
}

std::string GetImportedFuncStr(const ImportedFunc& value)
{
    std::stringstream ss;
    ss << "#from <" << value.GetSourcePackageName() << "> ";
    ss << "import " << value.GetAttributeInfo().ToString();
    if (value.IsFastNative()) {
        ss << "[fastNative] ";
    }
    if (value.IsCFFIWrapper()) {
        ss << "[CFFIWrapper] ";
    }
    ss << value.GetIdentifier();
    // params
    auto params = value.GetParamInfo();
    ss << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        ss << params[i].paramName;
        if (i != params.size() - 1) {
            ss << ", ";
        }
    }
    ss << ")";
    ss << ": " << value.GetType()->ToString();
    std::stringstream attrss;
    if (auto genericTypeParams = value.GetGenericTypeParams(); !genericTypeParams.empty()) {
        auto constraintsStr = GetGenericConstaintsStr(genericTypeParams);
        if (!constraintsStr.empty()) {
            attrss << "genericConstraints: " << constraintsStr;
        }
    }
    if (auto kind = value.GetFuncKind(); kind != FuncKind::DEFAULT) {
        if (attrss.str() != "") {
            attrss << ", ";
        }
        attrss << "kind: " << FUNCKIND_TO_STRING.at(kind);
    }
    auto annostr = value.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (value.GetGenericDecl() != nullptr) {
        attrss << ", genericDecl: " << value.GetGenericDecl()->GetIdentifierWithoutPrefix();
    }
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    if (auto anno = value.GetAnnoInfo(); anno.IsAvailable()) {
        ss << " funcAnnoInfo: " << anno.mangledName;
    }
    ss << " srcCodeIdentifier: " << value.GetSrcCodeIdentifier();
    if (auto rawMangledName = value.GetRawMangledName(); !rawMangledName.empty()) {
        ss << " rawMangledName: " << rawMangledName;
    }
    return ss.str();
}

std::string GetExceptionsStr(const std::vector<ClassType*>& exceptions)
{
    std::stringstream ss;
    ss << "[ ";
    if (exceptions.empty()) {
        ss << "ALL";
    } else {
        size_t i = 0;
        for (auto& e : exceptions) {
            ss << e->ToString();
            if (i != exceptions.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
    }
    ss << " ]";
    return ss.str();
}

void AddCommaOrNot(std::stringstream& ss)
{
    if (ss.str() != "") {
        ss << ", ";
    }
}

std::string PackageAccessLevelToString(const Package::AccessLevel& level)
{
    auto it = PACKAGE_ACCESS_LEVEL_TO_STRING_MAP.find(level);
    CJC_ASSERT(it != PACKAGE_ACCESS_LEVEL_TO_STRING_MAP.end());
    return it->second;
}

std::string CustomTypeKindToString(const CustomTypeDef& def)
{
    if (def.IsInterface()) {
        return "interface";
    } else if (def.IsClass()) {
        return "class";
    } else if (def.IsStruct()) {
        return "struct";
    } else if (def.IsEnum()) {
        return "enum";
    } else if (def.IsExtend()) {
        return "extend";
    }
    CJC_ABORT();
    return "";
}

std::string BoolToString(bool flag)
{
    if (flag) {
        return "true";
    } else {
        return "false";
    }
}
} // namespace Cangjie::CHIR
