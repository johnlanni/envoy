# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `881c7de95d` - Apply patch: 009-fix-wasm-npe.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 1 个

## Patch 修改概览

该 patch 主要修复 WASM 模块中的空指针异常（NPE）问题，核心修改是：
1. 将多处 `wasm()->isFailed()` 调用改为 `isFailed()`（在 HIGRESS 条件编译下）
2. 在 `sendLocalResponse` 中添加 HIGRESS 条件编译块，增强 details 信息

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `source/extensions/common/wasm/context.cc` | 行 1961-1967 | 架构差异 + 新增代码 |

## 自动合并成功的修改

以下修改已成功自动合并，无需手动处理：

| 修改点 | 位置 | 修改内容 |
|--------|------|----------|
| `onResolveDns` | ~行 240 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |
| `onStatsUpdate` | ~行 292 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |
| `onRedisCallSuccess` | ~行 1136 | `wasm()->isFailed()` → `isFailed()` |
| `onRedisCallFailure` | ~行 1166 | `wasm()->isFailed()` → `isFailed()` |
| `onHttpCallSuccess` | ~行 2169 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |
| `onHttpCallFailure` | ~行 2199 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |
| `onGrpcReceiveWrapper` | ~行 2235 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |
| `onGrpcCloseWrapper` | ~行 2277 | `wasm()->isFailed()` → `isFailed()` (HIGRESS) |

---

## 详细冲突分析与解决方案

### 1. `source/extensions/common/wasm/context.cc`

#### 冲突点 1 (行 1961-1967) - `sendLocalResponse` 函数

**冲突描述**: `sendLocalReply` 调用处的架构差异和 HIGRESS 条件编译块添加

**HEAD (1.36.4) 版本**:
```cpp
      absl::optional<Grpc::Status::GrpcStatus> grpc_status_code = absl::nullopt;
      if (grpc_status >= Grpc::Status::WellKnownGrpcStatus::Ok &&
          grpc_status <= Grpc::Status::WellKnownGrpcStatus::MaximumKnown) {
        grpc_status_code = Grpc::Status::WellKnownGrpcStatus(grpc_status);
      }
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status_code, details);
```
- ✅ 有 `grpc_status` → `grpc_status_code` 的类型转换（转成 `optional<GrpcStatus>`）
- ❌ 没有 HIGRESS 条件编译块（没有 wasm_details 增强）

**Incoming (1.27.7 patch) 版本**:
```cpp
#if defined(HIGRESS)
      auto wasm_details = absl::StrFormat("via_wasm%s%s", plugin_ ? "::" + plugin()->name_ : "",
                                          details.empty() ? "" : "::" + details);
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status, wasm_details);
#else
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status, details);
#endif
      local_reply_sent_ = true;
```
- ❌ 没有 `grpc_status_code` 转换（直接使用 `uint32_t grpc_status`）
- ✅ 有 HIGRESS 条件编译块（wasm_details 增强 details 信息）
- ✅ 包含 `local_reply_sent_ = true;`（1.36.4 已在后续代码中有，但位置不同）

---

## 🔴 架构差异警告

### 架构对比

| 组件 | 1.27.7 (源分支) | 1.36.4 (目标分支) |
|------|----------------|-------------------|
| `sendLocalReply` 第4参数类型 | `uint32_t grpc_status` | `absl::optional<Grpc::Status::GrpcStatus> grpc_status_code` |
| grpc_status 类型转换 | 无 | 有（uint32_t → optional） |

### 影响

1. **接口签名变化**: 1.36.4 的 `sendLocalReply` 接口期望 `optional<GrpcStatus>` 类型，而不是 `uint32_t`
2. **必须保留类型转换**: HIGRESS 分支也需要使用转换后的 `grpc_status_code`，不能直接用 `grpc_status`

---

## 解决方案

**策略**: 合并双方修改 - 保留 1.36.4 的类型转换 + 添加 HIGRESS 条件编译块

**解决后的代码**:
```cpp
      absl::optional<Grpc::Status::GrpcStatus> grpc_status_code = absl::nullopt;
      if (grpc_status >= Grpc::Status::WellKnownGrpcStatus::Ok &&
          grpc_status <= Grpc::Status::WellKnownGrpcStatus::MaximumKnown) {
        grpc_status_code = Grpc::Status::WellKnownGrpcStatus(grpc_status);
      }
#if defined(HIGRESS)
      auto wasm_details = absl::StrFormat("via_wasm%s%s", plugin_ ? "::" + plugin()->name_ : "",
                                          details.empty() ? "" : "::" + details);
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status_code, wasm_details);
#else
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status_code, details);
#endif
      local_reply_sent_ = true;
```

**关键点**:
1. ✅ 保留 `grpc_status_code` 类型转换逻辑（适配 1.36.4 接口）
2. ✅ 添加 HIGRESS 条件编译块（新增 wasm_details 功能）
3. ✅ HIGRESS 分支使用 `grpc_status_code`（不是 `grpc_status`）
4. ✅ 添加 `local_reply_sent_ = true;`

---

## 解决步骤总结

### ⚠️ 需要手动合并

1. `source/extensions/common/wasm/context.cc` (行 1961-1967)
   - 原因: 架构差异 - 需要合并类型转换逻辑和 HIGRESS 条件编译块

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟡 中 | `source/extensions/common/wasm/context.cc` | 需确保 `grpc_status_code` 在 HIGRESS 分支中正确使用 |

---

## 验证清单

- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 条件编译正确（HIGRESS 宏）
- [ ] `grpc_status_code` 类型在所有分支中使用正确

---

## 建议的工作流程

1. **第一阶段**: 按上述解决方案手动解决冲突
2. **第二阶段**: 编译验证 `//source/extensions/common/wasm:wasm_lib`
3. **第三阶段**: 运行相关单元测试
