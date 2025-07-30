// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares a BCHIR printer.
 */

#ifndef CANGJIE_CHIR_INTERRETER_BCHIRPRINTER_H
#define CANGJIE_CHIR_INTERRETER_BCHIRPRINTER_H

#include "cangjie/CHIR/Interpreter/BCHIR.h"
#include "cangjie/CHIR/Interpreter/OpCodes.h"
#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/Option/Option.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace Cangjie::CHIR::Interpreter {
class BCHIRPrinter {
public:
    BCHIRPrinter(std::ostream& os, const Bchir& bchir) : os(os), bchir(bchir)
    {
    }
    // This function is not used by the interpreter directly, but it's
    // necessary when debugging the interpreter runtime
    void Print(std::string header = "");
    // This prints out all relevant information that will be serialized.
    void PrintAll(std::string header = "");
    // Open (and create) a file for writing debug BCHIR information to for the
    // given package and compilation stage.
    static std::fstream GetBCHIROutputFile(
        const GlobalOptions& options, const std::string& fullPackageName, const std::string& stageName);

    /**
     * @brief DemangleName string in a more human readable format
     * i.e. _CN11std.FS$core9Exception6<init>ERN11std.FS$core6StringE
     *      std.core.Exception.init.std.core.String()
     **/
    static std::pair<std::string, std::string> DemangleName(const std::string& mangled)
    {
        const std::regex start("^_CN(\\d+)");
        const std::regex erCn("(ER)+_CN(\\d+)");
        const std::regex ecCn("(EC)+_CN(\\d+)");
        const std::regex end("H(v)*$");
        const std::regex rt("rt\\$");
        const std::regex std("std\\$");
        const std::regex fs("\\$");
        const std::regex cn("_CN(\\d+)");
        const std::regex inER(".initER");
        const std::regex initName("<init>");
        const std::regex mainName("<main>");
        auto ret = std::string(std::regex_replace(mangled, start, ""));
        ret = std::string(std::regex_replace(ret, initName, "init"));
        ret = std::string(std::regex_replace(ret, mainName, "main"));
        ret = std::string(std::regex_replace(ret, erCn, "."));
        ret = std::string(std::regex_replace(ret, ecCn, "."));
        ret = std::string(std::regex_replace(ret, end, ""));
        const std::regex num("(\\w)\\d+(?=\\w)");
        ret = std::string(std::regex_replace(ret, fs, "."));
        ret = std::string(std::regex_replace(ret, rt, "rt."));
        ret = std::string(std::regex_replace(ret, std, "std."));
        ret = std::string(std::regex_replace(ret, cn, ""));
        ret = std::string(std::regex_replace(ret, inER, ".init."));
        ret = std::string(std::regex_replace(ret, num, "$1."));
        std::size_t found = ret.find_first_of(".");
        std::string className = "default";
        std::string methodName = ret;
        if (found != std::string::npos) {
            className = ret.substr(0, found);
            methodName = ret.substr(found + 1, ret.size() - 1) + "()";
        }
        return std::make_pair(className, methodName);
    }

private:
    std::ostream& os;
    const Bchir& bchir;

    class DefinitionPrinter {
    public:
        DefinitionPrinter(const Bchir& bchir, const Bchir::Definition& def, std::ostream& os)
            : bchir(bchir), os(os), bytecode(def.GetByteCode()), def(def)
        {
        }
        void Print();

    private:
        const std::string LEFT{""};
        const std::string RIGHT{""};
        const std::string ARGSEP{", "};
        const std::string OPSEP{"\n"};
        void PrintOP();
        void PrintOPFloat();
        void PrintOPFloat8bytes();
        void PrintOPRune();
        void PrintOPSwitch();
        void PrintOPBinRshift(OpCode opCode);
        void PrintOPTypeCast();
        void PrintPath();
        void PrintOPSyscall();
        void PrintOPIntrinsic(OpCode opCode);
        void PrintOPCApply();
        // PrintOpCode advances index
        void PrintOpCode();
        void PrintAtIndex();
        void PrintAtIndex8bytes();

        // This function is not used by the interpreter directly, but it's
        // necessary when debugging the interpreter runtime
        void PrintTy();
        void Print(size_t argIndex, const std::string& str);

    private:
        const Bchir& bchir;
        std::ostream& os;
        const std::vector<Bchir::ByteCodeContent>& bytecode;
        const Bchir::Definition& def;
        size_t index{0};
    };

    // These methods are intended to print out BCHIR data
    // according to how it is serialized such that
    // we can debug BEP issues better later on.
    // This will need updates when new things are introduced to BCHIR.
    void PrintSClassTable();
    void PrintStrings();
    void PrintTypes();
    void PrintSourceFiles();
    static const std::unordered_map<Cangjie::OverflowStrategy, std::string> OVERFLOW_STRAT2STRING;
};

} // namespace Cangjie::CHIR::Interpreter

#endif // CANGJIE_CHIR_INTERRETER_BCHIRPRINTER_H
