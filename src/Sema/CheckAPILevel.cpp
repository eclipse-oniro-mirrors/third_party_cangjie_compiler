// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file provides the function of checking APILevel customized macros.
 */

#include "cangjie/Sema/CheckAPILevel.h"

#include <functional>
#include <iostream>
#include <stack>
#include <unordered_map>

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/StringConvertor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/SafePointer.h"

using namespace Cangjie;
using namespace AST;
using namespace APILevelCheck;

namespace {
const std::string PKG_NAME_WHERE_APILEVEL_AT = "ohos.labels";
const std::string APILEVEL_ANNO_NAME = "APILevel";
const std::string LEVEL_IDENTGIFIER = "level";
const std::string SYSCAP_IDENTGIFIER = "syscap";
const std::string CFG_PARAM_LEVEL_NAME = "APILevel_level";
const std::string CFG_PARAM_SYSCAP_NAME = "APILevel_syscap";
// Based on the definition of 'APILevel' in the 'ohos.labels'.
const std::string PARAMLIST_STR = "(UInt8, atomicservice!: Bool, crossplatform!: Bool, deprecated!: UInt8, form!: "
                                  "Bool, permission!: ?PermissionValue, stagemodelonly!: Bool, syscap!: String)";
// For level check:
const std::string PKG_NAME_DEVICE_INFO_AT = "ohos.device_info";
const std::string DEVICE_INFO = "DeviceInfo";
const std::string SDK_API_VERSION = "sdkApiVersion";
// For syscap check:
const std::string PKG_NAME_CANIUSE_AT = "ohos.base";
const std::string CANIUSE_IDENTIFIER = "canIUse";

LevelType Str2LevelType(std::string s)
{
    try {
        return static_cast<LevelType>(std::stoull(s));
    } catch (...) {
        return 0;
    }
}

void ParseLevel(const Expr& e, APILevelAnnoInfo& apilevel, DiagnosticEngine& diag)
{
    auto lce = DynamicCast<LitConstExpr*>(&e);
    if (!lce || lce->kind != LitConstKind::INTEGER) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_only_literal_support, e);
        return;
    }
    auto newLevel = Str2LevelType(lce->stringValue);
    apilevel.level = apilevel.level == 0 ? newLevel : std::min(newLevel, apilevel.level);
}

void ParseSysCap(const Expr& e, APILevelAnnoInfo& apilevel, DiagnosticEngine& diag)
{
    auto lce = DynamicCast<LitConstExpr>(&e);
    if (!lce || lce->kind != LitConstKind::STRING) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_only_literal_support, e);
        return;
    }
    apilevel.syscap = lce->stringValue;
}

void Placeholder([[maybe_unused]] const Expr& e, [[maybe_unused]] const APILevelAnnoInfo& apilevel,
    [[maybe_unused]] const DiagnosticEngine& diag)
{
}

using ParseNameParamFunc = std::function<void(const Expr&, APILevelAnnoInfo&, DiagnosticEngine&)>;
std::unordered_map<std::string, ParseNameParamFunc> parseNameParam = {
    {LEVEL_IDENTGIFIER, ParseLevel},
    {"atomicservice", Placeholder},
    {"crossplatform", Placeholder},
    {"deprecated", Placeholder},
    {"form", Placeholder},
    {"permission", Placeholder},
    {"stagemodelonly", Placeholder},
    {SYSCAP_IDENTGIFIER, ParseSysCap},
};

struct JsonObject;

struct JsonPair {
    std::string key;
    std::vector<std::string> valueStr;
    std::vector<OwnedPtr<JsonObject>> valueObj;
    std::vector<uint64_t> valueNum;
};

struct JsonObject {
    std::vector<OwnedPtr<JsonPair>> pairs;
};

enum class StringMod {
    KEY,
    VALUE,
};

std::string ParseJsonString(size_t& pos, const std::vector<uint8_t>& in)
{
    std::stringstream str;
    if (in[pos] == '"') {
        ++pos;
        while (pos < in.size() && in[pos] != '"') {
            str << in[pos];
            ++pos;
        }
    }

    return str.str();
}

uint64_t ParseJsonNumber(size_t& pos, const std::vector<uint8_t>& in)
{
    if (in[pos] < '0' || in[pos] > '9') {
        return 0;
    }
    std::stringstream num;
    while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') {
        num << in[pos];
        ++pos;
    }
    if (num.str().size()) {
        --pos;
    }
    try {
        return std::stoull(num.str());
    } catch (...) {
        return 0;
    }
}

OwnedPtr<JsonObject> ParseJsonObject(size_t& pos, const std::vector<uint8_t>& in);
void ParseJsonArray(size_t& pos, const std::vector<uint8_t>& in, Ptr<JsonPair> value)
{
    if (in[pos] != '[') {
        return;
    }
    ++pos;
    while (pos < in.size()) {
        if (in[pos] == ' ' || in[pos] == '\n') {
            ++pos;
            continue;
        }
        if (in[pos] == '"') {
            value->valueStr.emplace_back(ParseJsonString(pos, in));
        }
        if (in[pos] == '{') {
            value->valueObj.emplace_back(ParseJsonObject(pos, in));
        }
        if (in[pos] == ']') {
            return;
        }
        ++pos;
    }
}

OwnedPtr<JsonObject> ParseJsonObject(size_t& pos, const std::vector<uint8_t>& in)
{
    if (in[pos] != '{') {
        return nullptr;
    }
    ++pos;
    auto ret = MakeOwned<JsonObject>();
    auto mod = StringMod::KEY;
    while (pos < in.size()) {
        if (in[pos] == ' ' || in[pos] == '\n') {
            ++pos;
            continue;
        }
        if (in[pos] == '}') {
            return ret;
        }
        if (in[pos] == ':') {
            mod = StringMod::VALUE;
        }
        if (in[pos] == ',') {
            mod = StringMod::KEY;
        }
        if (in[pos] == '"') {
            if (mod == StringMod::KEY) {
                auto newData = MakeOwned<JsonPair>();
                newData->key = ParseJsonString(pos, in);
                ret->pairs.emplace_back(std::move(newData));
            } else {
                ret->pairs.back()->valueStr.emplace_back(ParseJsonString(pos, in));
            }
        }
        if (in[pos] >= '0' && in[pos] <= '9') {
            ret->pairs.back()->valueNum.emplace_back(ParseJsonNumber(pos, in));
        }
        if (in[pos] == '{') {
            // The pos will be updated to the pos of matched '}'.
            ret->pairs.back()->valueObj.emplace_back(ParseJsonObject(pos, in));
        }
        if (in[pos] == '[') {
            // The pos will be updated to the pos of matched ']'.
            ParseJsonArray(pos, in, ret->pairs.back().get());
        }
        ++pos;
    }
    return ret;
}

std::vector<std::string> GetJsonString(Ptr<JsonObject> root, const std::string& key)
{
    for (auto& v : root->pairs) {
        if (v->key == key) {
            return v->valueStr;
        }
        for (auto& o : v->valueObj) {
            auto ret = GetJsonString(o.get(), key);
            if (!ret.empty()) {
                return ret;
            }
        }
    }
    return {};
}

Ptr<JsonObject> GetJsonObject(Ptr<JsonObject> root, const std::string& key, const size_t index)
{
    for (auto& v : root->pairs) {
        if (v->key == key && v->valueObj.size() > index) {
            return v->valueObj[index].get();
        }
        for (auto& o : v->valueObj) {
            auto ret = GetJsonObject(o.get(), key, index);
            if (ret) {
                return ret;
            }
        }
    }
    return nullptr;
}

void ChkIfImportDeviceInfo(DiagnosticEngine& diag, const ImportManager& im, const IfAvailableExpr& iae)
{
    if (iae.GetFullPackageName() == PKG_NAME_DEVICE_INFO_AT) {
        return;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == PKG_NAME_DEVICE_INFO_AT) {
            return;
        }
    }
    auto builder = diag.DiagnoseRefactor(
        DiagKindRefactor::sema_use_expr_without_import, iae, PKG_NAME_DEVICE_INFO_AT, "IfAvailable");
    builder.AddNote("depend on declaration '" + DEVICE_INFO + "'");
}

void ChkIfImportBase(DiagnosticEngine& diag, const ImportManager& im, const IfAvailableExpr& iae)
{
    if (iae.GetFullPackageName() == PKG_NAME_CANIUSE_AT) {
        return;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == PKG_NAME_CANIUSE_AT) {
            return;
        }
    }
    auto builder =
        diag.DiagnoseRefactor(DiagKindRefactor::sema_use_expr_without_import, iae, PKG_NAME_CANIUSE_AT, "IfAvailable");
    builder.AddNote("depend on declaration '" + CANIUSE_IDENTIFIER + "'");
}

void ClearAnnoInfoOfDepPkg(ImportManager& importManager)
{
    auto clearAnno = [](Ptr<Node> node) {
        auto decl = DynamicCast<Decl>(node);
        if (!decl) {
            return VisitAction::WALK_CHILDREN;
        }
        auto isCustomAnno = [](auto& a) { return a->kind == AnnotationKind::CUSTOM; };
        decl->annotations.erase(
            std::remove_if(decl->annotations.begin(), decl->annotations.end(), isCustomAnno), decl->annotations.end());
        return VisitAction::WALK_CHILDREN;
    };
    auto cjdPaths = importManager.GetDepPkgCjdPaths();
    for (auto& cjdInfo : cjdPaths) {
        auto depPkg = importManager.GetPackage(cjdInfo.first);
        if (!depPkg) {
            continue;
        }
        Walker(depPkg, clearAnno).Walk();
    }
}
} // namespace

// Before desugar: `@IfAvaliable(level: 11, {=>...}, {=>...})`
// Desugar as: `if (DeviceInfo.sdkApiVersion >= 11) {...} else {...}`
OwnedPtr<Expr> APILevelAnnoChecker::DesugarIfAvailableLevelCondition(IfAvailableExpr& iae)
{
    auto deviceInfoDecl = importManager.GetImportedDecl(PKG_NAME_DEVICE_INFO_AT, DEVICE_INFO);
    if (!deviceInfoDecl) {
        ChkIfImportDeviceInfo(diag, importManager, iae);
        return nullptr;
    }
    // Get property 'DeviceInfo.sdkApiVersion.get()' from PKG_NAME_DEVICE_INFO_AT.
    Ptr<FuncDecl> target = nullptr;
    for (auto& member : deviceInfoDecl->GetMemberDecls()) {
        if (member->identifier.Val() == SDK_API_VERSION && member->astKind == ASTKind::PROP_DECL) {
            auto propDecl = StaticCast<PropDecl>(member.get());
            CJC_ASSERT(propDecl->getters.size() > 0);
            target = propDecl->getters[0].get();
        }
    }
    auto refOfDeviceInfo = CreateRefExpr(SrcIdentifier(DEVICE_INFO), deviceInfoDecl->ty, {});
    refOfDeviceInfo->SetTarget(deviceInfoDecl);
    auto me = CreateMemberAccess(std::move(refOfDeviceInfo), "$sdkApiVersionget");
    me->SetTarget(target);
    me->ty = target->ty;
    auto callExpr = CreateCallExpr(
        std::move(me), {}, target, typeManager.GetPrimitiveTy(TypeKind::TYPE_INT64), CallKind::CALL_DECLARED_FUNCTION);
    callExpr->SetTarget(target);
    auto condition = CreateBinaryExpr(std::move(callExpr), std::move(iae.GetArg()->expr), TokenKind::GE);
    condition->ty = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    AddCurFile(*condition, iae.curFile);
    return std::move(condition);
}

// Before desugar: `@IfAvaliable(syscap: "xxx", {=>...}, {=>...})`
// Desugar as: `if (canIUse("xxx")) {...} else {...}`
OwnedPtr<Expr> APILevelAnnoChecker::DesugarIfAvailableSyscapCondition(IfAvailableExpr& iae)
{
    // Get func decleration 'public func canIUse(syscap: String): Bool' from PKG_NAME_CANIUSE_AT.
    auto canIUseFunc = importManager.GetImportedDecl(PKG_NAME_CANIUSE_AT, CANIUSE_IDENTIFIER);
    if (!canIUseFunc || !canIUseFunc->IsFunc()) {
        ChkIfImportBase(diag, importManager, iae);
        return nullptr;
    }
    auto canIUseRef = CreateRefExpr(SrcIdentifier(CANIUSE_IDENTIFIER), canIUseFunc->ty, {});
    canIUseRef->SetTarget(canIUseFunc);
    std::vector<OwnedPtr<FuncArg>> argList;
    argList.emplace_back(CreateFuncArg(std::move(iae.GetArg()->expr)));
    auto condition = CreateCallExpr(std::move(canIUseRef), std::move(argList), StaticCast<FuncDecl>(canIUseFunc),
        typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN), CallKind::CALL_DECLARED_FUNCTION);
    AddCurFile(*condition, iae.curFile);
    condition->SetTarget(canIUseFunc);
    return std::move(condition);
}

OwnedPtr<Expr> APILevelAnnoChecker::DesugarIfAvailableCondition(IfAvailableExpr& iae)
{
    if (iae.GetArg()->name == LEVEL_IDENTGIFIER) {
        return DesugarIfAvailableLevelCondition(iae);
    } else if (iae.GetArg()->name == SYSCAP_IDENTGIFIER) {
        return DesugarIfAvailableSyscapCondition(iae);
    } else {
        return nullptr;
    }
}

void APILevelAnnoChecker::DesugarIfAvailableExprInTypeCheck(IfAvailableExpr& iae)
{
    if (iae.desugarExpr) {
        return;
    }
    // Create condition.
    OwnedPtr<Expr> condition = DesugarIfAvailableCondition(iae);
    if (!condition) {
        return;
    }
    auto ifBlock = ASTCloner::Clone(iae.GetLambda1()->funcBody->body.get());
    auto elseBlock = ASTCloner::Clone(iae.GetLambda2()->funcBody->body.get());
    CJC_ASSERT(ifBlock->ty == elseBlock->ty);
    auto ifExpr = CreateIfExpr(
        std::move(condition), std::move(ifBlock), std::move(elseBlock), iae.GetLambda1()->funcBody->body->ty);
    iae.desugarExpr = std::move(ifExpr);
}

void APILevelAnnoChecker::ParseJsonFile(const std::vector<uint8_t>& in) noexcept
{
    size_t startPos = static_cast<size_t>(std::find(in.begin(), in.end(), '{') - in.begin());
    auto root = ParseJsonObject(startPos, in);
    auto deviceSysCapObj = GetJsonObject(root, "deviceSysCap", 0);
    std::map<std::string, SysCapSet> dev2SyscapsMap;
    for (auto& subObj : deviceSysCapObj->pairs) {
        SysCapSet syscapsOneDev;
        for (auto path : subObj->valueStr) {
            std::vector<uint8_t> buffer;
            std::string failedReason;
            FileUtil::ReadBinaryFileToBuffer(path, buffer, failedReason);
            if (!failedReason.empty()) {
                diag.DiagnoseRefactor(
                    DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, path, failedReason);
                return;
            }
            startPos = static_cast<size_t>(std::find(buffer.begin(), buffer.end(), '{') - buffer.begin());
            auto rootOneDevice = ParseJsonObject(startPos, buffer);
            auto curSyscaps = GetJsonString(rootOneDevice, "SysCaps");
            for (auto syscap : curSyscaps) {
                if (Utils::NotIn(syscap, syscapsOneDev)) {
                    syscapsOneDev.emplace_back(syscap);
                }
            }
        }
        dev2SyscapsMap.emplace(subObj->key, syscapsOneDev);
    }
    std::optional<SysCapSet> lastSyscap = std::nullopt;
    for (auto& dev2Syscaps : dev2SyscapsMap) {
        SysCapSet& curSyscaps = dev2Syscaps.second;
        std::sort(curSyscaps.begin(), curSyscaps.end());
        SysCapSet intersection;
        if (lastSyscap.has_value()) {
            std::set_intersection(lastSyscap.value().begin(), lastSyscap.value().end(), curSyscaps.begin(),
                curSyscaps.end(), std::back_inserter(intersection));
        } else {
            intersection = curSyscaps;
        }
        lastSyscap = intersection;
        for (auto syscap : curSyscaps) {
            if (Utils::NotIn(syscap, unionSet)) {
                unionSet.emplace_back(syscap);
            }
        }
    }
    intersectionSet = lastSyscap.value();
    for (auto cap : intersectionSet) {
        Debugln("[apilevel] APILevel_syscap intersectionSet: ", cap);
    }
    for (auto cap : unionSet) {
        Debugln("[apilevel] APILevel_syscap unionSet: ", cap);
    }
}

void APILevelAnnoChecker::ParseOption() noexcept
{
    auto& option = ci.invocation.globalOptions;
    auto found = option.passedWhenKeyValue.find(CFG_PARAM_LEVEL_NAME);
    if (found != option.passedWhenKeyValue.end()) {
        globalLevel = Str2LevelType(found->second);
        Debugln("[apilevel] APILevel_level: ", globalLevel);
        optionWithLevel = true;
    }
    found = option.passedWhenKeyValue.find(CFG_PARAM_SYSCAP_NAME);
    if (found != option.passedWhenKeyValue.end()) {
        auto syscapsCfgPath = found->second;
        std::vector<uint8_t> jsonContent;
        std::string failedReason;
        FileUtil::ReadBinaryFileToBuffer(syscapsCfgPath, jsonContent, failedReason);
        if (!failedReason.empty()) {
            diag.DiagnoseRefactor(
                DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, syscapsCfgPath, failedReason);
            return;
        }
        ParseJsonFile(jsonContent);
        optionWithSyscap = true;
    }
}

bool APILevelAnnoChecker::IsAnnoAPILevel(Ptr<Annotation> anno, [[maybe_unused]] const Decl& decl)
{
    if (ctx && ctx->curPackage && ctx->curPackage->fullPackageName == PKG_NAME_WHERE_APILEVEL_AT) {
        return anno->identifier == APILEVEL_ANNO_NAME;
    }
    if (!anno || anno->identifier != APILEVEL_ANNO_NAME) {
        return false;
    }
    auto target = anno->baseExpr ? anno->baseExpr->GetTarget() : nullptr;
    if (target && target->curFile && target->curFile->curPackage &&
        target->curFile->curPackage->fullPackageName != PKG_NAME_WHERE_APILEVEL_AT) {
        return false;
    }
    return true;
}

APILevelAnnoInfo APILevelAnnoChecker::Parse(const Decl& decl)
{
    if (decl.annotations.empty()) {
        return APILevelAnnoInfo();
    }
    if (auto found = levelCache.find(&decl); found != levelCache.end()) {
        return found->second;
    }
    APILevelAnnoInfo ret;
    for (auto& anno : decl.annotations) {
        if (!anno || !IsAnnoAPILevel(anno.get(), decl)) {
            continue;
        }
        if (anno->args.size() < 1) {
            auto builder = diag.DiagnoseRefactor(
                DiagKindRefactor::sema_wrong_number_of_arguments, *anno, "missing argument", PARAMLIST_STR);
            builder.AddMainHintArguments(std::to_string(parseNameParam.size()), std::to_string(anno->args.size()));
            continue;
        }
        // level
        CJC_NULLPTR_CHECK(anno->args[0]);
        parseNameParam[LEVEL_IDENTGIFIER](*anno->args[0]->expr.get(), ret, diag);
        // syscap
        for (size_t i = 1; i < anno->args.size(); ++i) {
            std::string argName = anno->args[i]->name.Val();
            CJC_ASSERT(parseNameParam.count(argName) > 0);
            std::string preSyscap = ret.syscap;
            parseNameParam[argName](*anno->args[i]->expr.get(), ret, diag);
            if (!preSyscap.empty() && preSyscap != ret.syscap) {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_apilevel_multi_diff_syscap, decl);
            }
        }
        Debugln("[apilevel] ", decl.identifier, " get level: ", ret.level, ", syscap: ", ret.syscap);
    }
    levelCache[&decl] = ret;
    return ret;
}

bool APILevelAnnoChecker::CheckLevel(
    const Node& node, const Decl& target, const APILevelAnnoInfo& scopeAPILevel, bool reportDiag)
{
    if (!optionWithLevel) {
        return true;
    }
    LevelType scopeLevel = scopeAPILevel.level != 0 ? scopeAPILevel.level : globalLevel;
    auto targetAPILevel = Parse(target);
    if (targetAPILevel.level > scopeLevel && !node.begin.IsZero()) {
        if (reportDiag) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_apilevel_ref_higher, node, target.identifier.Val(),
                std::to_string(targetAPILevel.level), std::to_string(scopeLevel));
        }
        return false;
    }
    return true;
}

bool APILevelAnnoChecker::CheckSyscap(
    const Node& node, const Decl& target, const APILevelAnnoInfo& scopeAPILevel, bool reportDiag)
{
    if (!optionWithSyscap) {
        return true;
    }
    SysCapSet scopeSyscaps = unionSet;
    if (!scopeAPILevel.syscap.empty()) {
        scopeSyscaps.emplace_back(scopeAPILevel.syscap);
    }
    auto targetAPILevel = Parse(target);
    std::string targetLevel = targetAPILevel.syscap;
    if (targetLevel.empty()) {
        return true;
    }
    auto diagForSyscap = [this, &scopeSyscaps, &node, &targetLevel](DiagKindRefactor kind) {
        auto builder = diag.DiagnoseRefactor(kind, node, targetLevel);
        std::stringstream scopeSyscapsStr;
        // 3 is maximum number of syscap limit.
        for (size_t i = 0; i < std::min(scopeSyscaps.size(), static_cast<size_t>(3)); ++i) {
            std::string split = scopeSyscaps[i] == scopeSyscaps.back() ? "" : ", ";
            scopeSyscapsStr << scopeSyscaps[i] << split;
        }
        if (scopeSyscaps.size() > 3) {
            scopeSyscapsStr << "...";
        }
        builder.AddNote("the following syscaps are supported: " + scopeSyscapsStr.str());
    };

    auto found = std::find(scopeSyscaps.begin(), scopeSyscaps.end(), targetLevel);
    if (found == scopeSyscaps.end() && !node.begin.IsZero()) {
        if (reportDiag) {
            diagForSyscap(DiagKindRefactor::sema_apilevel_syscap_error);
        }
        return false;
    }

    scopeSyscaps = intersectionSet;
    if (!scopeAPILevel.syscap.empty()) {
        scopeSyscaps.emplace_back(scopeAPILevel.syscap);
    }
    found = std::find(scopeSyscaps.begin(), scopeSyscaps.end(), targetLevel);
    if (found == scopeSyscaps.end() && !node.begin.IsZero()) {
        if (reportDiag) {
            diagForSyscap(DiagKindRefactor::sema_apilevel_syscap_warning);
        }
        return false;
    }
    return true;
}

bool APILevelAnnoChecker::CheckNode(Ptr<Node> node, APILevelAnnoInfo& scopeAPILevel, bool reportDiag)
{
    if (!node) {
        return true;
    }
    auto target = node->GetTarget();
    if (auto ce = DynamicCast<CallExpr>(node); ce && ce->resolvedFunction) {
        target = ce->resolvedFunction;
    }
    if (!target) {
        return true;
    }
    bool ret = true;
    if (target->TestAttr(Attribute::CONSTRUCTOR) && target->outerDecl) {
        ret = CheckLevel(*node, *target->outerDecl, scopeAPILevel, reportDiag) && ret;
        ret = CheckSyscap(*node, *target->outerDecl, scopeAPILevel, reportDiag) && ret;
        if (!ret) {
            return false;
        }
    }
    ret = CheckLevel(*node, *target, scopeAPILevel, reportDiag) && ret;
    ret = CheckSyscap(*node, *target, scopeAPILevel, reportDiag) && ret;
    return ret;
}

void APILevelAnnoChecker::CheckIfAvailableExpr(IfAvailableExpr& iae, APILevelAnnoInfo& scopeAPILevel)
{
    Ptr<FuncArg> arg = iae.GetArg();
    if (!arg || !arg->expr || arg->expr->astKind != ASTKind::LIT_CONST_EXPR) {
        return;
    }
    if (parseNameParam.count(arg->name) <= 0) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_unknow_arg_name, MakeRange(iae.GetArg()->name),
            iae.GetArg()->name.Val());
        return;
    }
    auto ifScopeAPILevel = APILevelAnnoInfo();
    parseNameParam[arg->name](*arg->expr.get(), ifScopeAPILevel, diag);
    // if branch.
    auto checkerIf = [this, &ifScopeAPILevel, &scopeAPILevel](Ptr<Node> node) -> VisitAction {
        if (auto e = DynamicCast<IfAvailableExpr>(node)) {
            CheckIfAvailableExpr(*e, ifScopeAPILevel);
            return VisitAction::SKIP_CHILDREN;
        }
        // If the reference meets the 'IfAvaliable' condition but does not meet the global APILevel configuration, set
        // linkage to 'EXTERNAL_WEAK'.
        auto ret = CheckNode(node, ifScopeAPILevel);
        if (ret && !CheckNode(node, scopeAPILevel, false)) {
            if (node->GetTarget()) {
                Debugln("[apilevel] mark target ", node->GetTarget()->identifier.Val(), " as EXTERNAL_WEAK");
                node->GetTarget()->linkage = Linkage::EXTERNAL_WEAK;
            } else if (auto ce = DynamicCast<CallExpr>(node); ce && ce->resolvedFunction) {
                Debugln("[apilevel] mark function ", ce->resolvedFunction->identifier.Val(), " as EXTERNAL_WEAK");
                ce->resolvedFunction->linkage = Linkage::EXTERNAL_WEAK;
            }
        }
        if (!ret) {
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(iae.GetLambda1(), checkerIf).Walk();
    // else branch.
    auto checkerElse = [this, &scopeAPILevel](Ptr<Node> node) -> VisitAction {
        if (auto e = DynamicCast<IfAvailableExpr>(node)) {
            CheckIfAvailableExpr(*e, scopeAPILevel);
            return VisitAction::SKIP_CHILDREN;
        }
        CheckNode(node, scopeAPILevel);
        return VisitAction::WALK_CHILDREN;
    };
    Walker(iae.GetLambda2(), checkerElse).Walk();
}

void APILevelAnnoChecker::Check(Package& pkg)
{
    ctx = ci.GetASTContextByPackage(&pkg);
    std::vector<Ptr<Decl>> scopeDecl;
    auto checker = [this, &scopeDecl](Ptr<Node> node) -> VisitAction {
        if (auto decl = DynamicCast<Decl>(node)) {
            scopeDecl.emplace_back(decl);
            return VisitAction::WALK_CHILDREN;
        }
        auto scopeAPILevel = APILevelAnnoInfo();
        for (auto it = scopeDecl.rbegin(); it != scopeDecl.rend(); ++it) {
            scopeAPILevel = Parse(**it);
            if (scopeAPILevel.level != 0) {
                break;
            }
        }
        if (auto iae = DynamicCast<IfAvailableExpr>(node)) {
            scopeAPILevel.level = scopeAPILevel.level == 0 ? globalLevel : scopeAPILevel.level;
            CheckIfAvailableExpr(*iae, scopeAPILevel);
            return VisitAction::SKIP_CHILDREN;
        }
        if (!CheckNode(node, scopeAPILevel)) {
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    auto popScope = [&scopeDecl](Ptr<Node> node) -> VisitAction {
        if (!scopeDecl.empty() && scopeDecl.back() == node) {
            scopeDecl.pop_back();
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(&pkg, checker, popScope).Walk();

    // Desugar can't be skipped any node, cannot perform at the same time as the check.
    auto desugarIfAvailable = [this](Ptr<Node> node) {
        if (auto iae = DynamicCast<IfAvailableExpr>(node)) {
            DesugarIfAvailableExprInTypeCheck(*iae);
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(&pkg, desugarIfAvailable).Walk();
    // Clear the annotation information of the dependency package.
    ClearAnnoInfoOfDepPkg(importManager);
}
