// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// The Cangjie API is in Beta. For details on its capabilities and limitations, please refer to the README file.

package recorddecl

public struct Rectangle<T> {
    var width: Int32
    var length: Int32
    init(width!: Int32, length!: Int32) {
        this.width = width
        this.length = length
    }
}
