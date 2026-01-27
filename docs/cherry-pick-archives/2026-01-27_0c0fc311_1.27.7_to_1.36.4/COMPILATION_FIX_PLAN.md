# 编译修复计划

## 概述

Cherry-pick `0c0fc31187` (017-misc-opt-of-wasm-rebuild-mem-and-pb-cache.patch) 后，编译阶段遇到以下问题需要修复。

## 编译错误列表

### 1. Protobuf Hash Cache Patch 版本不兼容

**错误信息**:
```
Error applying patch /home/jz/higress-envoy-1.36/envoy/bazel/protobuf_hash_cache.patch: 
in patch applied to .../src/google/protobuf/BUILD.bazel: 
could not apply patch due to CONTENT_DOES_NOT_MATCH_TARGET, error applying change near line 504
```

**原因分析**:
- 原始 patch (`protobuf_hash_cache.patch`) 是针对 1.27.7 使用的旧版 protobuf 设计
- 1.36.4 使用 protobuf v29.3，API 和文件结构有变化
- BUILD.bazel 中依赖位置从第 504 行变更到第 681 行

**修复方案**:
1. 创建新的 `protobuf_hash_cache_v29.3.patch` 适配 protobuf v29.3
2. 更新 `bazel/repositories.bzl` 使用新的 patch 文件

**修复文件**:
- `bazel/protobuf_hash_cache_v29.3.patch` (新建)
- `bazel/repositories.bzl` (修改)

### 2. HashCachedMessageUtil 依赖问题

**问题**:
在 protobuf patch 适配过程中，`HashCachedMessageUtil` 类依赖 `GetCachedHashValue()` 方法。

**修复方案**:
- 在 `source/common/protobuf/utility.h` 中正确实现 `HashCachedMessageUtil`
- 确保 HIGRESS 条件编译正确

**修复文件**:
- `source/common/protobuf/utility.h`

### 3. Wasm shouldRebuild 机制适配

**问题**:
Patch 中的 `shouldRebuild()` / `setShouldRebuild()` 方法需要在 1.36.4 架构中实现。

**修复方案**:
1. 在 `Wasm` 类中添加 `should_rebuild_` 成员变量和 getter/setter
2. 在 `PluginConfig::maybeReloadHandleIfNeeded()` 中集成 shouldRebuild 检查逻辑

**修复文件**:
- `source/extensions/common/wasm/wasm.h`
- `source/extensions/common/wasm/wasm.cc` (在 cherry-pick 阶段已处理)

### 4. Filter Manager 编译问题

**问题**:
`filter_manager.cc` 中某些代码需要适配 1.36.4 架构。

**修复文件**:
- `source/common/http/filter_manager.cc`

## 修复统计

| 修复类型 | 文件数 | 新增行数 | 删除行数 |
|----------|--------|----------|----------|
| Protobuf patch 适配 | 2 | ~490 | 0 |
| Wasm 机制适配 | 2 | ~10 | ~5 |
| 其他编译修复 | 3 | ~20 | ~15 |

## 验证结果

- [x] `bazel build --config=clang //source/extensions/common/wasm/...` 通过
- [x] `bazel build --config=clang //source/common/listener_manager/...` 通过
- [x] `bazel build --config=clang //source/common/http/...` 通过
