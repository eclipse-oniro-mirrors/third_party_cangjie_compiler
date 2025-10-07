// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

/**
 * @file
 *
 * This file declares the DebugLocation in CHIR.
 */

#ifndef CANGJIE_CHIR_DEBUGLOCATION_H
#define CANGJIE_CHIR_DEBUGLOCATION_H

#include <string>
#include <vector>

namespace Cangjie::CHIR {
const std::string INVALID_NAME = ""; // Invalid file name

struct Position {
    unsigned line{0};
    unsigned column{0};

    bool IsLegal() const
    {
        return line != 0 && column != 0;
    }
    bool IsZero() const
    {
        return line == 0 && column == 0;
    }
};

/**
 * @brief A Debug location in source code.
 *
 */
class DebugLocation {
public:
    DebugLocation(const std::string& absPath, unsigned fileID, const Position& beginPos, const Position& endPos,
        const std::vector<int>& scopeInfo = {0})
        : absPath(&absPath), fileID(fileID), beginPos(beginPos), endPos(endPos), scopeInfo(scopeInfo)
    {
    }
    DebugLocation() : absPath(&INVALID_NAME), fileID(0), beginPos({0, 0}), endPos({0, 0})
    {
    }
    ~DebugLocation() = default;

    Position GetBeginPos() const
    {
        return beginPos;
    }

    Position GetEndPos() const
    {
        return endPos;
    }

    void SetBeginPos(const Position& pos)
    {
        beginPos = pos;
    }

    void SetEndPos(const Position& pos)
    {
        endPos = pos;
    }

    void SetScopeInfo(const std::vector<int>& scope)
    {
        scopeInfo = scope;
    }

    /**
     * @brief get the ID of the file.
     */
    unsigned GetFileID() const
    {
        return fileID;
    }

    const std::string& GetAbsPath() const
    {
        return *absPath;
    }

    std::vector<int> GetScopeInfo() const
    {
        return scopeInfo;
    }

    bool IsInvalidPos() const
    {
        return beginPos.line == 0 || beginPos.column == 0 || endPos.line == 0 || endPos.column == 0;
    }

    bool IsInvalidMacroPos() const
    {
        return beginPos.line == 0 || beginPos.column == 0;
    }

    std::string GetFileName() const
    {
#ifdef _WIN32
        const std::string dirSeparator = "\\/";
#else
        const std::string dirSeparator = "/";
#endif
        auto fileName = absPath->substr(absPath->find_last_of(dirSeparator) + 1);
        return fileName;
    }

    std::string ToString() const;

private:
    const std::string* absPath; /* the absolute path of file */
    unsigned fileID;            /* the file id */
    Position beginPos;          /* the begin position in file, start from 1, 1 */
    Position endPos;            /* the end position in file, start from 1, 1 */
    std::vector<int> scopeInfo; /* scope info, like 0-0-0 0-1 */
};

const DebugLocation INVALID_LOCATION = DebugLocation();
} // namespace Cangjie::CHIR
#endif // CANGJIE_CHIR_DEBUGLOCATION_H
