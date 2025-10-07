// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

#ifndef CANGJIE_MANGLE_ASTMANGLER_H
#define CANGJIE_MANGLE_ASTMANGLER_H

#include "cangjie/Mangle/BaseMangler.h"

namespace Cangjie {
inline const std::string MANGLE_PAREN_PREFIX = "P";
inline const std::string MANGLE_OPTION_PREFIX = "O";
inline const std::string MANGLE_AST_CFUNC_PREFIX = "CF";
inline const std::string MANGLE_FUNC_PREFIX = "F";
inline const std::string MANGLE_QUALIFIED_PREFIX = "Q";
inline const std::string MANGLE_INITIAL_PREFIX = "INITIAL";
inline const std::string MANGLE_EXIG_PREFIX = "Extend!";
inline const std::string MANGLE_THIS_PREFIX = "H";
inline const std::string MANGLE_PUBLIC_PREFIX = "P";
inline const std::string MANGLE_PROTECTED_PREFIX = "R";
inline const std::string MANGLE_PRIVATE_PREFIX = "I";
inline const std::string MANGLE_DIDOLLAR_PREFIX = "$$";
inline const std::string MANGLE_VARIABLE_PREFIX = "V";
inline const std::string MANGLE_LET_PREFIX = "L";
inline const std::string MANGLE_INOUT_PREFIX = "$IO";
inline const std::string MANGLE_GEXTEND_PREFIX = "W";

/**
 * @brief Name mangle class
 * Class ASTMangler is designed to manage mangling rules during Parser.
 *
 */
class ASTMangler {
public:
    /**
     * @brief The constructor of class ASTMangler.
     *
     * @param fullPackageName The full package name.
     * @return ASTMangler The instance of ASTMangler.
     */
    explicit ASTMangler(const std::string& fullPackageName);

    /**
     * @brief The destructor of class ASTMangler.
     */
    ~ASTMangler();

    /**
     * @brief Helper function for mangling decl name.
     *
     * @param decl The decl to be mangled.
     * @return std::string The mangled signature.
     */
    std::string Mangle(const AST::Decl& decl) const;

    /**
     * @brief Helper function for mangling primitive types name.
     *
     * @param type The primitive types do not have Decl*, the first overload is for source package, and
     * the second overload imported packages.
     * @return std::string The mangled signature.
     */
    static std::string ManglePrimitiveType(const AST::Type& type);

    /**
     * @brief Helper function for mangling builtin types name.
     *
     * @param type The builtin types to be mangled.
     * @return std::string The mangled signature.
     */
    static std::string MangleBuiltinType(const std::string& type);

    /**
     * @brief Get the extended type name.
     *
     * @param name The extended mangled name to be truncated.
     * @return std::optional<std::string> The extended type name.
     */
    static std::optional<std::string> TruncateExtendMangledName(const std::string& name);

private:
    struct ASTManglerImpl* pimpl;
};

} // namespace Cangjie
#endif // CANGJIE_MANGLE_ASTMANGLER_H
