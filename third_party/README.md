# 第三方库 patch 说明

## flatbufferPatch.diff

FlatBuffers 是一个高效的跨平台、跨语言序列化库，仓颉语言使用 FlatBuffers 库完成编译器数据到指定格式的序列化反序列化操作。为了提供 C++ 到仓颉语言的序列化反序列化能力，本项目对 FlatBuffers 库进行定制化修改，导出为 flatbufferPatch.diff 文件。

### Patch 应用方式

#### 方式 1：手动应用

下载 FlatBuffers v24.3.25 版本到 third_party 目录下：

```
git clone https://gitee.com/mirrors_trending/flatbuffers -b v24.3.25
```

在 third_party 目录下，执行命令应用 patch：
```
git apply --dictionary flatbuffers flatbufferPatch.diff
```

#### 方式 2：自动应用（推荐）

项目的构建命令中已集成自动化下载第三方库，应用 patch 的脚本，执行以下命令自动应用 patch 并构建：

```
python build.py build -t release # 脚本会自动并应用 patch
```

# 注意事项
此 patch 基于指定的官方版本 v24.3.25 开发，升级第三方库版本前需重新验证 patch 有效性，避免版本不兼容导致的问题。

# llvmPatch.diff
当使用选项"--use-oh-llvm-repo"时，仓颉可以通过使用llvmPatch.diff进行构建。
llvmPatch通过仓库https://gitcode.com/Cangjie/llvm-project/ project的main分支基于llvmorg-15.0.4生成。
当仓颉发布版本时，llvmPatch.diff文件会自动生成并更新。