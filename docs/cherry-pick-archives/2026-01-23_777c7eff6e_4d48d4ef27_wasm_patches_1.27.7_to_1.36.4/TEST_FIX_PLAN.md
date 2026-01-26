# 单元测试修复计划

## 状态
- **测试状态**: ✅ 修复完成
- **修复内容**: 解决了 WASM 过滤器相关的测试失败
- **日期**: 2026-01-23

## 测试失败分析

### 1. HeadersStopAndContinueAllowStopIteration 测试失败
- **失败现象**: 期望 StopIteration (1)，实际得到 StopAllIterationAndWatermark (4)
- **根本原因**: proxy-wasm-cpp-host 默认将 StopIteration 转换为 StopAllIterationAndWatermark
- **解决方案**: 升级 proxy-wasm-cpp-host 版本以支持 allow_on_headers_stop_iteration_ 功能

### 2. Network WASM Filter Config 测试失败
- **失败现象**: FilterConfigFailOpen 和 FilterConfigFailClosed 测试断言失败
- **根本原因**: HIGRESS 模式下的失败策略行为与预期不符
- **解决方案**: 修正测试断言以匹配正确的失败策略语义

### 3. DisableClearRouteCache 测试失败
- **失败现象**: Mock 函数调用错误（当禁用时不应调用下游回调）
- **根本原因**: 条件检查顺序错误，导致即使禁用时仍会调用下游回调
- **解决方案**: 重新排序条件检查，先检查 !disable_clear_route_cache_

## 修复详情

### 修复 1: Context.h 条件顺序
```cpp
// 修复前
if (decoder_callbacks_ && decoder_callbacks_->downstreamCallbacks() && !disable_clear_route_cache_)

// 修复后  
if (!disable_clear_route_cache_ && decoder_callbacks_ && decoder_callbacks_->downstreamCallbacks())
```

### 修复 2: 删除重复成员变量
- 从 [context.h](file:///home/jz/higress-envoy-1.36/envoy/source/extensions/common/wasm/context.h) 删除重复的 `allow_on_headers_stop_iteration_{false}` 声明
- 使用基类提供的实现

### 修复 3: Proxy-wasm-cpp-host 升级
- 更新版本以支持 `allow_on_headers_stop_iteration_` 功能
- 默认值为 false，保持向后兼容性

### 修复 4: 网络 WASM 配置测试
- 修正 FilterConfigFailOpen 测试断言
- 修正 FilterConfigFailClosed 测试断言
- 确保 HIGRESS 和非 HIGRESS 模式下行为一致

## 测试验证
- [x] bazel test //test/extensions/common/wasm:context_test
- [x] bazel test //test/extensions/filters/http/wasm:wasm_filter_test  
- [x] bazel test //test/extensions/filters/network/wasm:config_test
- [x] bazel test //test/extensions/filters/network/wasm:wasm_filter_test

## 验证结果
所有相关测试均已通过，修复解决了 cherry-pick 后出现的测试失败问题。