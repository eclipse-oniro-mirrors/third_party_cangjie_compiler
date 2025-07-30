// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_USERBASE_H
#define CANGJIE_USERBASE_H

#ifdef CANGJIE_WRITE_PROFILE
#include "cangjie/Utils/CheckUtils.h"
#endif

#include <fstream>
#include <iostream>
#include <string>

namespace Cangjie {
class UserBase {
public:
    enum class OutType { STRING, JSON };
    virtual std::string GetResult([[maybe_unused]] const OutType& oType) const
    {
        if (!enable) {
            return "";
        }
#ifdef CANGJIE_WRITE_PROFILE
        if (oType == OutType::JSON) {
            return GetJson();
        }
#endif
        return GetFlat();
    };
    void OutputResult(const OutType& oType) const noexcept
    {
        if (!enable) {
            return;
        }
        try {
            std::string result = GetResult(oType);
#ifdef CANGJIE_WRITE_PROFILE
            CJC_ASSERT(oType == OutType::JSON);
            WriteJson(result, GetSuffix());
#else
            printf("%s\n", result.c_str());
#endif
        } catch (...) {
            std::cerr << "Get an exception while running function 'OutputResult' !!!\n" << std::endl;
        }
    }
#ifdef CANGJIE_WRITE_PROFILE
    void WriteJson(const std::string& context, const std::string& suffix) const
    {
        std::string name = packageName;
        size_t startPos = name.find('/');
        if (startPos != std::string::npos) {
            (void)name.replace(startPos, 1, "-");
        }
        std::string filename = name + suffix;
        std::ofstream out(filename.c_str());
        if (!out) {
            return;
        }
        out.write(context.c_str(), static_cast<long>(context.size()));
        out.close();
    }
#endif

    virtual ~UserBase() = default;

    void Enable(bool en)
    {
        enable = en;
    }
    bool IsEnable() const
    {
        return enable;
    }
    void SetPackageName(const std::string& name)
    {
        packageName = name;
    }

protected:
    bool enable{false};
    std::string packageName;

private:
#ifdef CANGJIE_WRITE_PROFILE
    virtual std::string GetSuffix() const = 0;
    virtual std::string GetJson() const = 0;
#endif
    virtual std::string GetFlat() const = 0;
};
} // namespace Cangjie

#endif // CANGJIE_USERBASE_H
