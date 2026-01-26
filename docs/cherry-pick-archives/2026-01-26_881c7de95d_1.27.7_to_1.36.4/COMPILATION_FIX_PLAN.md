# 编译修复计划

## 编译错误

```
source/extensions/common/wasm/context.cc:1963:7: error: use of undeclared identifier 'local_reply_sent_'
      local_reply_sent_ = true;
      ^
```

## 错误原因分析

### 架构差异

| 组件 | 1.27.7 (源分支) | 1.36.4 (目标分支) |
|------|----------------|-------------------|
| 变量定义 | `bool local_reply_sent_ = false;` | 不存在 |
| 保护检查 | `if (local_reply_sent_) { return; }` | 不存在 |
| 类似变量 | `local_reply_hold_` | `failure_local_reply_sent_` |

### 1.27.7 中的完整保护逻辑

```cpp
wasm()->addAfterVmCallAction([...] {
      // When the wasm vm fails, failStream() is called if the plugin is fail-closed, we need
      // this flag to avoid calling sendLocalReply() twice.
      if (local_reply_sent_) {
        return;
      }
      // ... sendLocalReply call ...
      local_reply_sent_ = true;
    });
```

### 1.36.4 中的现状

- 没有 `local_reply_sent_` 变量
- sendLocalResponse 中没有重复调用保护机制
- 使用 `failure_local_reply_sent_` 仅用于 failStream 场景

## 修复方案

**策略**: 移除 `local_reply_sent_ = true;`

**原因**:
1. 本次 patch 的核心目的是添加 HIGRESS 条件编译块（wasm_details）和 `isFailed()` 修改
2. `local_reply_sent_` 保护机制需要同时添加变量定义和检查逻辑，超出本次 patch 范围
3. 1.36.4 可能依赖其他机制处理重复调用（或上层调用者保证不会重复）

## 修复代码

**修改前**:
```cpp
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

**修改后**:
```cpp
#if defined(HIGRESS)
      auto wasm_details = absl::StrFormat("via_wasm%s%s", plugin_ ? "::" + plugin()->name_ : "",
                                          details.empty() ? "" : "::" + details);
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status_code, wasm_details);
#else
      decoder_callbacks_->sendLocalReply(static_cast<Envoy::Http::Code>(response_code), body_text,
                                         modify_headers, grpc_status_code, details);
#endif
```

## 风险评估

| 风险 | 说明 | 缓解措施 |
|------|------|----------|
| 重复调用 sendLocalReply | 理论上可能发生，但依赖上层保证 | 1.36.4 原本就没有此保护 |
