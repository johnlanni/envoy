# Cherry-pick 归档总结

## 基本信息

- **日期**: 2026-01-27
- **原始提交**: `0c0fc31187` - Apply patch: 017-misc-opt-of-wasm-rebuild-mem-and-pb-cache.patch
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **工作分支**: cherry-pick/0c0fc311-to-1.36.4
- **操作人**: Cursor AI Assistant

## Patch 功能概述

该 patch 实现以下功能：
1. **Wasm VM 重建机制增强**: 区分 crash recovery (`rebuild(true)`) 和主动 memory rebuild (`rebuild(false)`)
2. **Protobuf Hash 缓存**: 优化 protobuf message 的 hash 计算性能
3. **PLUGIN_VM_MEMORY property**: 新增获取 Wasm VM 内存使用量的属性
4. **Request Headers Fallback**: 支持从 StreamInfo 获取请求头

## 冲突概览

- **冲突文件数**: 9
- **编译问题**: 是 (Protobuf patch 版本不兼容)
- **单测问题**: 是 (sizeof 断言需更新)

### 冲突文件解决策略

| 文件 | 策略 | 说明 |
|------|------|------|
| conn_manager_impl.cc | 采用 Incoming | API 已存在 |
| filter_chain_manager_impl.cc | 采用 Incoming | HashCachedMessageUtil 已存在 |
| context.cc | 采用 Incoming | 新增 PLUGIN_VM_MEMORY |
| wasm.cc | 手动合并 | recover() → rebuild(bool) |
| wasm.h | 手动合并 | recover() → rebuild(bool) |
| http/wasm_filter.h | 采用 HEAD | 架构已重构到 PluginConfig |
| network/wasm_filter.h | 采用 HEAD | 架构已重构到 PluginConfig |
| stream_info_impl_test.cc | 合并双方 | sizeof 值列表 |
| listener_manager_impl.cc.orig | 删除 | 备份文件 |

## 关键决策

### 1. Wasm recover() → rebuild(bool) 重命名

**决策**: 将 `PluginHandleSharedPtrThreadLocal::recover()` 重命名为 `rebuild(bool is_fail_recovery)`

**原因**:
- 原 recover() 在 1.36.4 中未被调用，可安全重命名
- 新方法区分 crash recovery 和主动 rebuild，更灵活
- 保留 HEAD 的成员变量命名 (handle 而非 handle_)

### 2. Protobuf Hash Cache Patch 适配

**决策**: 创建新的 `protobuf_hash_cache_v29.3.patch` 适配 protobuf v29.3

**原因**:
- 原 patch 针对旧版 protobuf，无法应用于 v29.3
- protobuf v29.3 的 BUILD.bazel 和 API 结构有变化

### 3. shouldRebuild 机制适配位置

**决策**: 在 `PluginConfig::maybeReloadHandleIfNeeded()` 中集成 shouldRebuild 检查

**原因**:
- 1.36.4 的 FilterConfig 继承 PluginConfig，createFilter 逻辑已移到基类
- maybeReloadHandleIfNeeded() 是现有的重载处理点

### 4. http/wasm_filter.h 和 network/wasm_filter.h 采用 HEAD

**决策**: 不合并 patch 中的 createFilter() 实现

**原因**:
- 1.36.4 已将 createFilter 重构为 PluginConfig::createContext()
- patch 的 shouldRebuild 逻辑需要在 PluginConfig 层实现

## 注意事项

1. **Protobuf Patch 维护**: 当 Envoy 升级 protobuf 版本时，需同步更新 protobuf_hash_cache_v29.3.patch
2. **rebuild_total Counter**: 已存在于 stats_handler.h 的 LIFECYCLE_STATS 宏中
3. **HIGRESS 条件编译**: 所有 HashCachedMessageUtil 和 rebuild 相关代码都在 HIGRESS 条件下

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] TEST_FIX_PLAN.md - 测试修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改文件列表

## 提交记录

| 提交 | 说明 |
|------|------|
| 492ff3ff81 | Apply patch: 017-misc-opt-of-wasm-rebuild-mem-and-pb-cache.patch |
| aa97309333 | Fix: adapt cherry-pick to 1.36.4 architecture |
