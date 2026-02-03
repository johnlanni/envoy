# Cherry-pick 冲突解决方案

## 概述

- **提交**: 8bd37c666e0a8f5615198bcc82369f63647e183a
- **标题**: Optimize wasm fail recovery rebuild logic
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/8bd37c66-to-envoy-1.36
- **冲突状态**: ✅ 无冲突，自动合并成功

## 提交描述

Skip old_handle_.expired() check for fail recovery scenarios to ensure
recovery always proceeds when the VM crashes. This prevents fail recovery
from being blocked by lingering old handle references.

## 修改的文件

- `source/extensions/common/wasm/wasm.cc`

## 冲突分析

### 无冲突

✅ Cherry-pick 成功完成，git 自动合并了所有更改。

### 代码更改内容

**文件**: `source/extensions/common/wasm/wasm.cc`

**更改位置**: 第 230 行

**更改内容**:
```cpp
// 原代码（envoy-1.27）:
if (!old_handle_.expired()) {

// 目标代码（envoy-1.36，已应用 patch）:
if (!is_fail_recovery && !old_handle_.expired()) {
```

**说明**: 在条件判断中添加 `!is_fail_recovery &&`，使得在故障恢复场景下跳过对 old_handle 的检查，确保恢复过程可以顺利进行。

## ALIMESH → HIGRESS 宏替换检查

✅ **已检查**: 源提交和目标文件中均无 ALIMESH 宏，无需替换。

目标分支已使用 HIGRESS 宏（参见第 206、214 行）。

## 风险评估

- **风险等级**: 🟢 低
- **影响范围**: Wasm 插件故障恢复逻辑
- **向后兼容性**: ✅ 兼容，仅优化故障恢复行为
- **测试需求**: 需验证相关单元测试

## 验证清单

- [x] Cherry-pick 成功
- [x] 代码逻辑已正确应用
- [x] ALIMESH 宏检查完成（无需替换）
- [x] 编译验证 - ✅ 通过
- [x] 单元测试验证 - ✅ 通过

## 编译验证结果

**编译目标**: `//source/extensions/common/wasm:wasm_lib`

**结果**: ✅ 编译成功

```
INFO: Build completed successfully, 2 total actions
```

## 单元测试验证结果

**测试目标**: `//test/extensions/common/wasm:wasm_test`

**结果**: ✅ 测试通过

```
Executed 1 out of 1 test: 1 test passes.
```

**说明**: 由于 `rebuild` 函数是 HIGRESS 特有功能（在 `#if defined(HIGRESS)` 条件编译块内），上游 Envoy 没有针对此功能的特定测试。运行了 wasm_test 以确保没有引入回归。

## 完成状态

✅ Cherry-pick 成功完成，所有验证通过。
