# Cherry-pick 归档总结

## 基本信息

- **日期**: 2026-02-03
- **原始提交**: 8bd37c666e0a8f5615198bcc82369f63647e183a
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/8bd37c66-to-envoy-1.36
- **新提交**: a0253862996cd3d6c118c273bf16be4eafd9a2da
- **操作人**: zhangty

## 提交概述

**标题**: Optimize wasm fail recovery rebuild logic

**描述**: Skip old_handle_.expired() check for fail recovery scenarios to ensure recovery always proceeds when the VM crashes. This prevents fail recovery from being blocked by lingering old handle references.

## 冲突概览

- **冲突状态**: ✅ 无冲突，自动合并成功
- **冲突文件数**: 0
- **修改文件数**: 1
- **编译问题**: 否
- **单测问题**: 否

## 修改的文件

1. `source/extensions/common/wasm/wasm.cc`

## 代码更改详情

### source/extensions/common/wasm/wasm.cc (第 230 行)

在 `PluginHandleSharedPtrThreadLocal::rebuild` 函数中，修改条件判断逻辑：

**更改前**:
```cpp
if (!old_handle_.expired()) {
```

**更改后**:
```cpp
if (!is_fail_recovery && !old_handle_.expired()) {
```

**目的**: 在故障恢复场景下跳过对 `old_handle_.expired()` 的检查，确保 VM 崩溃时恢复过程能够顺利进行，防止故障恢复被残留的旧 handle 引用阻塞。

## 关键决策

1. **无需手动解决冲突**: Git 自动合并成功，目标分支架构与源分支兼容。

2. **ALIMESH → HIGRESS 宏替换**: 检查确认源提交中不包含 ALIMESH 宏，目标分支已正确使用 HIGRESS 宏。

3. **HIGRESS 特有功能**: `rebuild` 函数整个在 `#if defined(HIGRESS)` 条件编译块内，是 HIGRESS 特有的故障恢复功能，上游 Envoy 中不存在此功能。

4. **测试策略**: 由于是 HIGRESS 特有功能且上游无对应测试，运行了通用的 `wasm_test` 确保没有引入回归问题。

## 验证结果

### 编译验证

**目标**: `//source/extensions/common/wasm:wasm_lib`

**结果**: ✅ 通过

```
INFO: Build completed successfully, 2 total actions
```

### 单元测试验证

**目标**: `//test/extensions/common/wasm:wasm_test`

**结果**: ✅ 通过

```
Executed 1 out of 1 test: 1 test passes.
```

## 注意事项

1. **条件编译**: 此功能仅在定义了 HIGRESS 宏时生效，不影响标准 Envoy 构建。

2. **故障恢复语义**: 这个修改改变了故障恢复时的行为 - 即使旧 handle 仍被引用，也会继续创建新 VM 进行恢复。这是预期的行为变更，目的是优先保证服务可用性。

3. **内存考虑**: 在非故障恢复场景下，仍会检查 `old_handle_.expired()` 以防止 VM 累积导致的内存问题。

## 风险评估

- **风险等级**: 🟢 低
- **影响范围**: 仅影响 HIGRESS 构建的 Wasm 故障恢复逻辑
- **向后兼容性**: ✅ 完全兼容
- **回滚难度**: 🟢 容易（单文件单行修改）

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md
- [x] commit_info.txt
- [x] modified_files.txt
- [ ] COMPILATION_FIX_PLAN.md (无编译问题)
- [ ] TEST_FIX_PLAN.md (无测试失败)

## 时间线

- **2026-02-03**: 创建工作分支 `cherry-pick/8bd37c66-to-envoy-1.36`
- **2026-02-03**: Cherry-pick 成功，无冲突
- **2026-02-03**: 编译验证通过
- **2026-02-03**: 单元测试验证通过
- **2026-02-03**: 文档归档完成

## 后续操作建议

1. 将工作分支 `cherry-pick/8bd37c66-to-envoy-1.36` 合并到 `envoy-1.36`
2. 推送到远程仓库
3. 如有 HIGRESS 特定的集成测试，建议运行完整的故障恢复测试场景

## 增量更新记录

| 日期 | 更新内容 | 原因 |
|------|---------|------|
| - | - | - |
