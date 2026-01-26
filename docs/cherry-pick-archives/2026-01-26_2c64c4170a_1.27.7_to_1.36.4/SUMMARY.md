# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|------|
| **日期** | 2026-01-26 |
| **原始提交** | `2c64c4170a` |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **Patch 名称** | 011-wasm-abi-support-injectencodeonheader.patch |

## 功能概述

此 patch 为 WASM ABI 添加了 `injectEncodedDataToFilterChainOnHeader` 功能支持：
- 新增 `Context::injectEncodedDataToFilterChainOnHeader()` 方法
- 新增 `inject_encoded_data.proto` 定义
- 新增两个 Foreign Function: `inject_encoded_data_to_filter_chain` 和 `inject_encoded_data_to_filter_chain_on_header`

## 冲突概览

| 项目 | 值 |
|------|------|
| 冲突文件数 | 2 |
| 编译问题 | 是 (已修复) |
| 单测问题 | 否 |

### 冲突文件

1. `source/extensions/common/wasm/ext/BUILD` - 2 个冲突块
2. `source/extensions/common/wasm/foreign.cc` - 1 个冲突块

## 关键决策

1. **BUILD 文件冲突解决**: 保留两者 - 1.36.4 的 `set_envoy_filter_state_*` 和 patch 的 `inject_encoded_data_*` proto 定义都需要
2. **foreign.cc include 冲突解决**: 合并所有 include 语句 - 添加 `inject_encoded_data.pb.h` 到现有 include 列表
3. **编译修复**: 将 lambda 捕获从 `[=]` 改为 `[=, this]`，适配 clang-18 对隐式 this 捕获的 deprecated 警告

## 编译适配

### 问题
```
error: implicit capture of 'this' with a capture default of '=' is deprecated
```

### 原因
1.36.4 使用 clang-18，对 lambda 中使用 `[=]` 隐式捕获 `this` 会产生 deprecated 警告，而项目配置将此警告视为错误。

### 修复
```cpp
// 修复前
encoder_callbacks_->dispatcher().post([=]() {

// 修复后
encoder_callbacks_->dispatcher().post([=, this]() {
```

## 测试验证

- ✅ `//test/extensions/common/wasm:context_test` - 通过
- ✅ `//test/extensions/common/wasm:foreign_test` - 通过

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改文件列表
