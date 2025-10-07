// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares the Searcher related classes.
 */

#ifndef CANGJIE_AST_SEACHER_H
#define CANGJIE_AST_SEACHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include "cangjie/AST/Query.h"
#include "cangjie/AST/Symbol.h"

namespace Cangjie {
class ASTContext;

/**
 * The Node of @p Trie tree, which contains the information of the next node and the value, prefix, suffix of current
 * node. If current node is the end of a path which represents a string, the @p rawValue contains the value of the
 * string.
 */
class TrieNode {
public:
    using Next = std::map<char, std::unique_ptr<TrieNode>>;
    Next next;                                      /**< Next trie node map. */
    char c;                                         /**< Trie node's value. */
    std::string value;                              /**< Store string value at the leaf node. */
    std::string prefix;                             /**< Store prefix string of current node. */
    std::set<std::string> suffixes;                 /**< Store suffix strings of current node. */
    std::set<AST::Symbol*> ids;                     /**< Symbol ID set. */
    uint32_t depth = 0;                             /**< Depth of the node in the tree. */
    TrieNode* parent{nullptr};                      /**< Pointer to the parent node. */
    explicit TrieNode(char c) : c(c)
    {
    }
};

/**
 * @p Trie is a class which contains a tree whose single path represents a string. Trie tree is used to do prefix and
 * suffix string pattern match.
 */
class Trie {
public:
    std::unique_ptr<TrieNode> root;
    Trie()
    {
        root = std::make_unique<TrieNode>('R');
    }
    explicit Trie(const std::string& value)
    {
        root = std::make_unique<TrieNode>('R');
        Insert(value);
    }

    void Reset()
    {
        root = std::make_unique<TrieNode>('R');
    }
    void Reset(const std::string& value)
    {
        root = std::make_unique<TrieNode>('R');
        Insert(value);
    }
    /**
     * Insert a string, the prefix string and the suffix strings of current
     * node are stored in @c prefix and @c suffixes.
     * The value of the given string is stored in @c value at the leaf node.
     * @param value String value to be inserted.
     */
    void Insert(const std::string& value);
    /**
     * Insert a string, the prefix string and the suffix strings of current
     * node are stored in @c prefix and @c suffixes.
     * The value of the given string is stored in @c value at the leaf node.
     * @param value String value to be inserted.
     * @param id Symbol id.
     */
    void Insert(const std::string& value, AST::Symbol& id);
    /**
     * Delete the node that has been inserted by insert method above.
     */
    void Delete(const std::string& value, AST::Symbol& id);
    /**
     * Get the root node to begin traverse for prefix search.
     * @param prefix Prefix search string.
     * @return The last node of the @p prefix string, when input @a0b, will point to the node of b.
     */
    TrieNode* GetPrefixRootNode(const std::string& prefix) const;
    /**
     * Traverse from the prefix root node to the bottom, get value from the leaf and add to match results.
     * @param prefix Prefix search string.
     */
    std::vector<std::string> PrefixMatch(const std::string& prefix) const;
    /**
     * Traverse from the root node to the bottom, when suffix value matched, concatenate the prefix and suffix, add to
     * match results.
     * @param suffix Suffix search string.
     */
    std::vector<std::string> SuffixMatch(const std::string& suffix) const;
};

/**
 * Simplified range query realization.
 */
class PosSearchApi {
public:
    /**
     * Cast position to a string monotonously.
     */
    static std::string PosToStr(const Position& pos);

    /**
     * Find the common root TrieNode of the given position in PosTrie.
     */
    static TrieNode* FindCommonRootInPosTrie(const Trie& posTrie, const Position& pos);

    /**
     * Find the common root TrieNode of the given position string in PosTrie.
     */
    static TrieNode* FindCommonRootInPosTrie(const Trie& posTrie, const std::string& pos);

    /**
     * Get IDs of symbols which is within a position range.
     */
    static std::set<AST::Symbol*> GetIDsWithinPosXByRange(const Trie& posTrie, const Position& startPos,
        const Position& endPos, bool isLeftClose = true, bool isRightClose = false);

    /**
     * Get IDs of symbols which is greater than the @p pos.
     * @param pos The position to compare.
     * @param isClose Default is true, [pos, ...).
     * @return Symbol IDs which are greater than @p pos.
     */
    static std::set<AST::Symbol*> GetIDsGreaterThanPos(const Trie& posTrie, const Position& pos, bool isClose = true);

    /**
     * Get IDs of symbols which is less than the @p pos.
     * @param pos The position to compare.
     * @param isClose Default is true, (..., pos].
     * @return Symbol IDs which are less than @p pos.
     */
    static std::set<AST::Symbol*> GetIDsLessThanPos(const Trie& posTrie, const Position& pos, bool isClose = true);
    static void GetIDsLessThanPosOfDepth(
        TrieNode& n, const std::string& posStr, std::set<AST::Symbol*>& ids, bool& isClose);

    const static int MAX_DIGITS_FILE = 4;   /**< Max num of files to compile once is 9999. */
    const static int MAX_DIGITS_LINE = 5;   /**< Max num of lines of a file is 99999. */
    const static int MAX_DIGITS_COLUMN = 5; /**< Max column width is 99999. */
    constexpr static int MAX_DIGITS_POSITION = MAX_DIGITS_FILE + MAX_DIGITS_LINE + MAX_DIGITS_COLUMN;
    constexpr static int MAX_LINE = static_cast<int>(10e5);
    const static int MAX_COLUMN = static_cast<int>(10e3);
};

using Order = std::function<bool(const AST::Symbol*, const AST::Symbol*)>;

/**
 * Sort functions.
 */
struct Sort {
    /*
     * Sort by position ascending order.
     */
    static Order posAsc;
    /*
     * Sort by position descending order.
     */
    static Order posDesc;
};

/**
 * AST Searcher will find the AST node by Query.
 */
class Searcher {
public:
    /**
     * Search symbol table in @p ctx, according to the @p query.
     */
    std::vector<AST::Symbol*> Search(const ASTContext& ctx, const std::string& query,
        const Order& order = Sort::posDesc, const std::unordered_set<uint64_t>& fileHashes = {});
    std::vector<AST::Symbol*> Search(const ASTContext& ctx, const Query* query, const Order& order = Sort::posDesc);
    void InvalidateCache()
    {
        cache.clear();
    }
    /**
     * Clear all caches except @p query.
     */
    void InvalidateCacheBut(const std::string& query);
    std::vector<std::string> GetScopeNamesByPrefix(const ASTContext& ctx, const std::string& prefix) const;
    void SetCache(std::unordered_map<std::string, std::vector<AST::Symbol*>>& newCache)
    {
        cache = newCache;
    }
    std::unordered_map<std::string, std::vector<AST::Symbol*>> GetCache()
    {
        return cache;
    }

private:
    unsigned long StrToUint(const ASTContext& ctx, const std::string& queryVal) const;
    // Get the IDS of symbols in symbol tables whose position equal to the given @p pos.
    std::set<AST::Symbol*> GetIDsByPosEQ(
        const ASTContext& ctx, const Position& pos, bool isLeftClose = true, bool isRightClose = true) const;
    // Get the IDS of symbols in symbol tables whose position is less than the given @p pos. If @contain is true, also
    // return the symbols contain the @p pos.
    std::set<AST::Symbol*> GetIDsByPosLT(const ASTContext& ctx, const Position& pos, bool contain = false) const;
    // Get the IDS of symbols in symbol tables whose position is greater than the given @p pos. If @contain is true,
    // also return the symbols contain the @p pos.
    std::set<AST::Symbol*> GetIDsByPosGT(const ASTContext& ctx, const Position& pos, bool contain = false) const;
    std::set<AST::Symbol*> GetIDsByName(const ASTContext& ctx, const std::string& name) const;
    std::set<AST::Symbol*> GetIDsByNamePrefix(const ASTContext& ctx, const std::string& prefix) const;
    std::set<AST::Symbol*> GetIDsByNameSuffix(const ASTContext& ctx, const std::string& suffix) const;
    std::set<AST::Symbol*> GetIDsByScopeName(const ASTContext& ctx, const std::string& scopeName) const;
    std::set<AST::Symbol*> GetIDsByScopeLevel(const ASTContext& ctx, uint32_t scopeLevel) const;
    std::set<AST::Symbol*> GetIDsByASTKind(const ASTContext& ctx, const std::string& astKind) const;
    std::vector<std::string> GetAstKindsBySuffix(const ASTContext& ctx, const std::string& suffix) const;
    std::set<AST::Symbol*> PerformSearch(const ASTContext& ctx, const Query& query);
    std::set<AST::Symbol*> GetIDs(const ASTContext& ctx, const Query& query) const;
    std::set<AST::Symbol*> GetIDsByPos(const ASTContext& ctx, const Query& query) const;
    std::set<AST::Symbol*> GetIDsByName(const ASTContext& ctx, const Query& query) const;
    std::set<AST::Symbol*> GetIDsByScopeLevel(const ASTContext& ctx, const Query& query) const;
    std::set<AST::Symbol*> GetIDsByScopeName(const ASTContext& ctx, const Query& query) const;
    std::set<AST::Symbol*> GetIDsByASTKind(const ASTContext& ctx, const Query& query) const;
    static std::set<AST::Symbol*> Intersection(const std::set<AST::Symbol*>& set1, const std::set<AST::Symbol*>& set2);
    static std::set<AST::Symbol*> Union(const std::set<AST::Symbol*>& set1, const std::set<AST::Symbol*>& set2);
    std::set<AST::Symbol*> Difference(const std::set<AST::Symbol*>& set1, const std::set<AST::Symbol*>& set2) const;
    bool InFiles(const std::unordered_set<uint64_t>& files, const AST::Symbol& id) const;
    std::pair<std::vector<AST::Symbol*>, bool> FindInSearchCache(
        const Query& query, const std::string& normalizedQuery);
    std::vector<AST::Symbol*> FilterAndSortSearchResult(
        const std::set<AST::Symbol*>& ids, const Query& query, const Order& order) const;
    std::unordered_map<std::string, std::vector<AST::Symbol*>> cache;
    friend class PosSearchApi;
};
} // namespace Cangjie

#endif
