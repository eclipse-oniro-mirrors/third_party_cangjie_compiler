// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#include "ASTDiff.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/IncrementalCompilation/CompilationCache.h"
#include "CompilationCacheSerialization.h"

using namespace Cangjie;
using namespace Cangjie::AST;
using namespace Cangjie::IncrementalCompilation;

namespace {
class OrderDiffCompare {
public:
    // `compareExclude`: exclude these decls when comparing order. In current implementation, these are added decls.
    // parameter `mod`: extract info of added top-level and member OOEAffectedDecl from
    OrderDiffCompare(const RawMangled2DeclMap& mangled2Decl, const CachedFileMap& cachedFileMap,
        const FileMap& curFileMap, const ModifiedDecls& mod)
        : md{mangled2Decl}, exc{GetAddedDecls(mod)}
    {
        for (auto& [file, l1]: cachedFileMap) {
            auto it = curFileMap.find(file);
            if (it == curFileMap.cend()) {
                continue;
            }
            auto& l2 = it->second;
            for (auto decl : CompareOrders(l1, l2)) {
                orderChanges.push_back(decl);
            }
        }
        CachedFileMap::mapped_type e{};
        for (auto& [file, l2]: curFileMap) {
            if (cachedFileMap.count(file) == 0) {
                for (auto decl : CompareOrders(e, l2)) {
                    orderChanges.push_back(decl);
                }
            }
        }
    }

    std::list<const Decl*> orderChanges;

private:
    static std::unordered_set<RawMangledName> GetAddedDecls(const ModifiedDecls& mod)
    {
        std::unordered_set<RawMangledName> res;
        for (auto decl : mod.added) {
            CJC_NULLPTR_CHECK(decl);
            // Raw mangle of direct extend not contain pakge name, so it's recorded decl maybe imported, skip it
            if (IsImported(*decl)) {
                continue;
            }
            if (IsOOEAffectedDecl(*decl)) { // OOEAffectedDecl never has member decl
                res.insert(decl->rawMangleName);
                continue;
            }
            for (auto member : GetMembers(*decl)) {
                if (IsOOEAffectedDecl(*member)) {
                    res.insert(member->rawMangleName);
                    continue;
                }
                for (auto mem1 : GetMembers(*member)) {
                    if (IsOOEAffectedDecl(*mem1)) {
                        res.insert(decl->rawMangleName);
                    }
                }
            }
        }

        for (auto& decl : mod.types) {
            // Raw mangle of direct extend not contain pakge name, so it's recorded decl maybe imported, skip it
            if (IsImported(*decl.first)) {
                continue;
            }
            for (auto member : GetMembers(*decl.first)) {
                if (IsOOEAffectedDecl(*member)) {
                    res.insert(member->rawMangleName);
                    continue;
                }
                for (auto mem1 : GetMembers(*member)) {
                    if (IsOOEAffectedDecl(*mem1)) {
                        res.insert(decl.first->rawMangleName);
                    }
                }
            }
        }
        return res;
    }

    static bool IsGV(const Decl& decl)
    {
        return IsGlobalOrStaticVar(decl) || decl.astKind == ASTKind::VAR_WITH_PATTERN_DECL;
    }

    std::set<const Decl*> CompareOrders(const CachedFileMap::mapped_type& old,
        const FileMap::mapped_type& cur)
    {
        using I = FileMap::mapped_type::size_type;
        // gvid in the second compilation
        std::unordered_map<RawMangledName, int> h{};
        for (I i{0}; i < cur.size(); ++i) {
            h[cur[i]->rawMangleName] = cur[i]->hash.gvid;
        }
        // maxids[i+1] denotes max of
        //      maxids[j], where j < i && old[j] is gv
        //      id[old[i]], the order at which the decl old[i] appears in cur
        // maxids[0] = 0 is a dummy
        std::vector<int> maxids{0};
        int last{0};
        for (auto& b: old) {
            // not found in cur, this decl is removed from this file, skipped
            if (auto it = h.find(b); it == h.cend()) {
                continue;
            } else {
                int maxid = std::max(it->second, last);
                maxids.push_back(maxid);
                auto decl = md.at(b);
                if (IsGV(*decl)) { // only gv has impact on other decls and therefore changes maxid of following decls
                    last = maxid;
                }
            }
        }

        // result part I: if any decl preceding the decl in the cache has larger gvid than it in incremental, the decl
        // needs recompile
        std::set<const Decl*> res;
        I rid{0};
        for (I i{0}; i < old.size(); ++i) {
            if (auto it = h.find(old[i]); it == h.cend()) {
                continue;
            } else {
                if (it->second < maxids[++rid]) {
                    res.insert(md.at(old[i]));
                }
            }
        }

        // store the names in the old file, used to check that a decl is moved to this file
        std::unordered_set<RawMangledName> oldNames{old.cbegin(), old.cend()};

        // result part II: for a decl that is moved to this file, recompiles it; if there is any decl preceding it in
        // incremental, they need recompile
        I i = cur.size();
        while (i > 0) {
            i--;
            // added decls are excluded
            if (exc.count(cur[i]->rawMangleName) == 1) {
                continue;
            }
            // name exists in old file, skip
            if (oldNames.count(cur[i]->rawMangleName) == 1) {
                continue;
            }
            /* Theoretically only other decls before it need recompile if a decl changes the file it belongs to.
               However, circular dependency among files can be formed with file change only.
               For example, in old compile:
                    file 1: a, b(use a)
                    file 2: c, d(use c)
               in new compile:
                    file 1: d, a
                    file 2: b, c
                since the decls moved are b and d, and there are no decls before them respectively, no decl is to be
                recompiled. This is however incorrect, because in the new compilation a circular depdency on the two
                files emerge. By recompiling all decls that have file change, this error is fixed.
            */
            res.insert(cur[i]);
            // file changed gv, add all decls preceding it into recompile
            if (IsGV(*cur[i])) {
                // this new gv, however, need not recompile
                for (I j{0}; j < i; ++j) {
                    res.insert(cur[j]);
                }
                break;
            }
        }
        return res;
    }

    const RawMangled2DeclMap& md;
    std::unordered_set<RawMangledName> exc;
};

class ASTDiffImpl {
public:
    std::pair<bool, bool> srcInfo;
    ModifiedDecls ret{};
    RawMangled2DeclMap mangled2Decl;

    explicit ASTDiffImpl(const std::pair<bool, bool>& srcInfo) : srcInfo{srcInfo}
    {
    }

    void CompareCurPkgASTCache(const ASTCache& cached, const ASTCache& cur, const CachedFileMap& cachedFileMap,
        const FileMap& curFileMap)
    {
        CompareCommonDecls(cached, cur);
        // const decls are to be recompiled even when no change
        OrderDiffCompare cp{mangled2Decl, cachedFileMap, curFileMap, ret};
        ret.orderChanges = std::move(cp.orderChanges);
    }

    void CompareImportedASTCache(const ASTCache& cached, const ASTCache& cur)
    {
        CompareCommonDecls(cached, cur);
    }

private:
    void CompareCommonDecls(const ASTCache& cached, const ASTCache& cur)
    {
        for (auto& [mangled, topDecl] : cached) {
            auto curIt = cur.find(mangled);
            if (curIt == cur.end()) {
                CollectDeletedTopLevelDecl(mangled, topDecl);
                continue;
            }
            auto& curDecl{curIt->second};
            CompareTopLevelDecl(topDecl, curDecl, mangled);
        }
        for (auto& [mangled, _] : cur) {
            if (auto cachedIt = cached.find(mangled); cachedIt == cached.end()) {
                CollectAddedTopLevelDecl(*mangled2Decl.at(mangled));
            }
        }
    }

    std::tuple<ASTCache, std::unordered_set<Ptr<const AST::Decl>>> CollectSourcePackage(const Package& p)
    {
        ASTCacheCalculator pc{p, srcInfo};
        pc.Walk();
        mangled2Decl = std::move(pc.mangled2Decl);
        return {std::move(pc.ret), std::move(pc.duplicatedMangleNames)};
    }

    void CollectAddedTopLevelDecl(const AST::Decl& decl)
    {
        ret.added.push_back(&decl);
    }

    void CollectDeletedTopLevelDecl(const RawMangledName& mangled, const TopLevelDeclCache& topDecl)
    {
        if (topDecl.astKind == static_cast<uint8_t>(ASTKind::TYPE_ALIAS_DECL)) {
            ret.deletedTypeAlias.push_back(mangled);
        } else {
            ret.deletes.push_back(mangled);
        }
        for (auto& p : topDecl.members) {
            ret.deletes.push_back(p.rawMangle);
        }
    }

    void CompareMemberDecl(TypeChange& typeChange, const MemberDeclCache& cached, const MemberDeclCache& cur)
    {
        CommonChange r{};
        r.decl = mangled2Decl.at(cur.rawMangle);
        r.srcUse = cached.srcUse != cur.srcUse;
        r.body = cached.bodyHash != cur.bodyHash;
        r.sig = cached.sigHash != cur.sigHash;
        if (r) {
            typeChange.changed.push_back(r);
        }
        CompareMembers(typeChange, cached.members, cur.members);
    }

    // compare all members of two decls. the order of decls is ignored
    void CompareMembers(
        TypeChange& typeChange, const std::vector<MemberDeclCache>& cached, const std::vector<MemberDeclCache>& cur)
    {
        auto sortedCache = ToSorted(cached, [](auto& a, auto& b) { return a.rawMangle < b.rawMangle; });
        auto sortedCur = ToSorted(cur, [](auto& a, auto& b) { return a.rawMangle < b.rawMangle; });
        auto cachedIt = sortedCache.cbegin();
        auto curIt = sortedCur.cbegin();
        while (cachedIt != sortedCache.cend() && curIt != sortedCur.cend()) {
            if ((*cachedIt)->rawMangle < (*curIt)->rawMangle) {
                typeChange.del.push_back((*cachedIt)->rawMangle);
                for (auto childDel : (*cachedIt)->members) {
                    typeChange.del.push_back(childDel.rawMangle);
                }
                ++cachedIt;
                continue;
            }
            if ((*cachedIt)->rawMangle > (*curIt)->rawMangle) {
                typeChange.added.push_back(mangled2Decl.at((*curIt++)->rawMangle));
                continue;
            }
            CompareMemberDecl(typeChange, **cachedIt++, **curIt++);
        }
        while (cachedIt != sortedCache.cend()) {
            typeChange.del.push_back((*cachedIt)->rawMangle);
            // For deleted property, we should also add the getter/setter into the list
            for (auto childDel : (*cachedIt)->members) {
                typeChange.del.push_back(childDel.rawMangle);
            }
            ++cachedIt;
        }
        while (curIt != sortedCur.cend()) {
            typeChange.added.push_back(mangled2Decl.at((*curIt++)->rawMangle));
        }
    }

    TypeChange CompareTypeDecl(const TopLevelDeclCache& cached, const TopLevelDeclCache& cur)
    {
        TypeChange typeChange{};
        typeChange.instVar = cached.instVarHash != cur.instVarHash;
        typeChange.virtFun = cached.virtHash != cur.virtHash;
        typeChange.sig = cached.sigHash != cur.sigHash;
        typeChange.srcUse = cached.srcUse != cur.srcUse;
        typeChange.body = cached.bodyHash != cur.bodyHash;
        CompareMembers(typeChange, cached.members, cur.members);
        // compare extends with typerel
        return typeChange;
    }

    static CommonChange CommonCompare(const TopLevelDeclCache& a, const TopLevelDeclCache& b, const AST::Decl& decl)
    {
        return {&decl, a.sigHash != b.sigHash, a.srcUse != b.srcUse, a.bodyHash != b.bodyHash};
    }

    void CompareTopLevelDecl(
        const TopLevelDeclCache& cachedDecl, const TopLevelDeclCache curDecl, const RawMangledName& mangled)
    {
        auto decl = mangled2Decl.at(mangled);
        switch (decl->astKind) {
            case ASTKind::TYPE_ALIAS_DECL:
                if (cachedDecl.sigHash != curDecl.sigHash) {
                    ret.aliases.push_back(RawStaticCast<const TypeAliasDecl*>(decl));
                }
                break;
            case ASTKind::FUNC_DECL:
            case ASTKind::MACRO_DECL:
            case ASTKind::MAIN_DECL:
            case ASTKind::VAR_DECL:
            case ASTKind::VAR_WITH_PATTERN_DECL:
                if (auto change = CommonCompare(cachedDecl, curDecl, *decl)) {
                    (void)ret.commons.emplace(decl, std::move(change));
                }
                break;
            default:
                if (auto change = CompareTypeDecl(cachedDecl, curDecl)) {
                    (void)ret.types.emplace(RawStaticCast<const InheritableDecl*>(decl), std::move(change));
                }
                break;
        }
    }
};
}

namespace Cangjie {
std::ostream& operator<<(std::ostream& out, const GlobalVarIndex& id)
{
    return out << '(' << id.file << ", " << id.id << ')';
}
}

namespace Cangjie::IncrementalCompilation {
ASTDiffResult ASTDiff(ASTDiffArgs&& args)
{
    ASTDiffImpl impl{std::make_pair(args.op.enableCompileDebug, args.op.displayLineInfo)};
    // Move source package mangled2Decl into imported package and use it.
    for (std::move_iterator it{args.rawMangleName2DeclMap.begin()};
        it != std::move_iterator{args.rawMangleName2DeclMap.end()}; ++it) {
        (void)args.importedMangled2Decl.emplace(*it);
    }
    impl.mangled2Decl = std::move(args.importedMangled2Decl);

    // compare source package
    impl.CompareCurPkgASTCache(
        args.prevCache.curPkgASTCache, args.astCacheInfo, args.prevCache.fileMap, args.curFileMap);
    // compare imported packages
    impl.CompareImportedASTCache(args.prevCache.importedASTCache, args.curImports);
    return {std::move(impl.ret), std::move(impl.mangled2Decl)};
}
}
