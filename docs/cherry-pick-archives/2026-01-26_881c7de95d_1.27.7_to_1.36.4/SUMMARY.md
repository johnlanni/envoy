# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **日期** | 2026-01-26 |
| **原始提交** | `881c7de95d` |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **Patch 名称** | 009-fix-wasm-npe.patch |

## Patch 功能

修复 WASM 模块中的空指针异常（NPE）问题：
- 将多处 `wasm()->isFailed()` 调用改为 `isFailed()`（在 HIGRESS 条件编译下）
- 在 `sendLocalResponse` 中添加 HIGRESS 条件编译块，增强 details 信息

## 冲突概览

| 项目 | 状态 |
|------|------|
| 冲突文件数 | 1 |
| 编译问题 | 是（已修复） |
| 单测问题 | 否 |

## 架构差异

| 组件 | 1.27.7 | 1.36.4 |
|------|--------|--------|
| `sendLocalReply` 参数类型 | `uint32_t grpc_status` | `optional<GrpcStatus> grpc_status_code` |
| `local_reply_sent_` 变量 | 存在 | 不存在（使用 `failure_local_reply_sent_`） |

## 关键决策

1. **grpc_status 类型适配**: 保留 1.36.4 的 `grpc_status_code` 类型转换逻辑，HIGRESS 分支也使用转换后的类型
2. **移除 local_reply_sent_**: 由于 1.36.4 没有此变量，且保护机制需要完整实现（变量+检查+设置），超出本次 patch 范围，故移除

## 修复内容

### 冲突解决
- 在 `sendLocalResponse` 函数中添加 HIGRESS 条件编译块（wasm_details）
- 适配 1.36.4 的 `grpc_status_code` 类型

### 编译修复
- 移除不存在的 `local_reply_sent_ = true;` 语句

## 验证结果

| 验证项 | 结果 |
|--------|------|
| 编译 | ✅ 通过 |
| context_test | ✅ 通过 |

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改文件列表
