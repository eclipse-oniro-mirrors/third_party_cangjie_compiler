// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

macro package define

import std.ast.*

public macro M(input: Tokens): Tokens {
    input
}

public macro M1(attr: Tokens, input: Tokens) {
    quote($attr;$input)
}

public macro B1(input: Tokens): Tokens {
    quote(1;2)
}
