# Cherry-pick 归档总结

## 基本信息

- **日期**: 2026-02-03
- **原始提交**: 8232d4ecfdf2f3d5de935fc817f1f04c905ace78
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/8232d4ec-to-envoy-1.36
- **最终提交**: b7e82675ba
- **操作人**: zhangty

## Cherry-pick 概览

### 功能描述

支持 Wasm 过滤器在 HTTP 和 Redis 调用中的分布式追踪（distributed tracing）。

### 冲突情况

**冲突文件数**: 0（无冲突，自动合并成功）

所有文件都能自动合并：
- `envoy/redis/async_client.h` - 自动合并
- `source/common/redis/async_client_impl.cc` - 自动合并
- `source/common/redis/async_client_impl.h` - 自动合并
- `source/extensions/common/wasm/context.cc` - 自动合并
- `test/extensions/filters/http/wasm/wasm_filter_test.cc` - 自动合并
- `test/mocks/redis/mocks.h` - 自动合并

### 测试问题

**编译问题**: 否  
**单测问题**: 是（3个测试分片失败，已修复）

## 关键决策

### 决策 1: ALIMESH 宏替换为 HIGRESS

**问题**: Cherry-pick 后，`source/extensions/common/wasm/context.cc` 中使用了 `ALIMESH` 宏，但目标分支使用 `HIGRESS` 宏。

**决策**: 将 `#if defined(ALIMESH)` 替换为 `#if defined(HIGRESS)`（第20行）

**原因**:
- 目标分支（envoy-1.36）统一使用 HIGRESS 宏
- 测试文件中所有条件编译都使用 `#if defined(HIGRESS)`
- Redis 相关函数（redisInit, redisCall）也使用 HIGRESS 宏
- 保持代码库一致性

### 决策 2: 移除 httpCall 中的 ALIMESH 宏包围

**问题**: 原始提交中 httpCall 的 tracing 代码被 `#ifdef ALIMESH` 包围，导致测试失败。

**决策**: 移除 `#ifdef ALIMESH` 和 `#endif`（第1109-1126行），使 tracing 代码无条件编译。

**原因**:
- 目标分支中 redisCall 的类似代码没有宏包围
- 测试期望 tracing 功能无条件启用
- 体现架构变化：envoy-1.27（功能门控）→ envoy-1.36（默认启用）
- 与测试断言一致（测试中没有 `#ifdef` 保护）

## 测试失败与修复

### 失败的测试

**测试目标**: `//test/extensions/filters/http/wasm:wasm_filter_test`  
**失败分片**: 3/50 (shards 4, 5, 6)  
**失败用例**: `RuntimesAndLanguages/WasmHttpFilterTest.AsyncCall/v8_cpp`

### 失败原因

测试期望 `options.child_span_name_` 被设置为 `"wasm plugin_name httpcall to cluster"`，但实际值为空字符串。

根因：httpCall 中设置 child_span_name 的代码被 `#ifdef ALIMESH` 包围，而测试环境未定义该宏。

### 修复方案

1. 将 `ALIMESH` 宏替换为 `HIGRESS`（保持与代码库一致）
2. 移除 httpCall tracing 代码的 `#ifdef ALIMESH` 包围（使功能默认启用）

### 修复验证

- ✅ 编译成功: `bazel build --config=clang //source/extensions/common/wasm:wasm_lib`
- ✅ AsyncCall 测试通过: `bazel test --test_filter="*AsyncCall*"`
- ✅ 完整测试套件通过: 所有 50 个分片全部通过

## 技术要点

### Tracing 功能实现

**HTTP Call Tracing**:
```cpp
// Set parent span from current stream context
if (proxy_wasm::current_context_ != nullptr) {
  auto* current_context = static_cast<Context*>(proxy_wasm::current_context_);
  if (current_context->decoder_callbacks_) {
    auto& span = current_context->decoder_callbacks_->activeSpan();
    options.setParentSpan(span);
  } else if (current_context->encoder_callbacks_) {
    auto& span = current_context->encoder_callbacks_->activeSpan();
    options.setParentSpan(span);
  }
}

// Set child span name
if (plugin()) {
  std::string child_span_name = absl::StrCat("wasm ", plugin()->name_, " httpcall to ", cluster_string);
  options.setChildSpanName(child_span_name);
}
```

**Redis Call Tracing**: 类似实现，使用 `RedisRequestOptions`

### 架构差异

| 特性 | envoy-1.27 (源) | envoy-1.36 (目标) |
|------|----------------|------------------|
| Tracing 宏控制 | `#ifdef ALIMESH` | 无宏控制（默认启用） |
| 宏命名 | ALIMESH | HIGRESS |
| Redis include | `#if defined(ALIMESH)` | `#if defined(HIGRESS)` |

## 注意事项

### 宏一致性

在目标分支中：
- ✅ 使用 `HIGRESS` 而非 `ALIMESH`
- ✅ Tracing 功能默认启用，不需要宏控制
- ✅ 只有 Redis async client include 需要 HIGRESS 宏保护

### 测试覆盖

所有修改的代码都有相应的测试覆盖：
- HTTP call tracing: `WasmHttpFilterTest.AsyncCall`
- Redis call tracing: `WasmHttpFilterTest.RedisCall`（在 HIGRESS 宏保护下）

## 相关文件

- [x] commit_info.txt - 提交详细信息
- [x] modified_files.txt - 修改文件列表
- [x] TEST_FIX_PLAN.md - 测试修复计划（包含详细的根因分析和修复过程）

## 后续行动

1. ✅ 修复已完成并验证
2. ⏭️ 可以将工作分支合并到目标分支 `envoy-1.36`
3. 📋 建议：审查其他文件中是否有 ALIMESH 宏需要替换

## 学习要点

1. **跨版本 Cherry-pick 的挑战**: 宏定义可能在不同版本间发生变化
2. **条件编译的演变**: 功能从门控（gated）到默认启用的迁移
3. **测试驱动修复**: 通过失败测试快速定位架构差异
4. **一致性原则**: 保持代码库内部命名和模式的一致性
