# 编译修复计划

## 编译错误

```
source/extensions/common/wasm/context.cc:1985:7: error: implicit capture of 'this' with a capture default of '=' is deprecated [-Werror,-Wdeprecated-this-capture]
```

## 错误分析

**文件**: `source/extensions/common/wasm/context.cc`
**位置**: 第 1983-1985 行

**问题代码**:
```cpp
encoder_callbacks_->dispatcher().post([=]() {
  auto buffer = ::Envoy::Buffer::OwnedImpl(body_text_copy);
  encoder_callbacks_->injectEncodedDataToFilterChain(buffer, end_stream);
});
```

**原因**: 
- 1.27.7 分支使用的编译器版本较低，没有此警告
- 1.36.4 分支使用 clang-18，对 lambda 中使用 `[=]` 隐式捕获 `this` 会产生 deprecated 警告
- 项目配置将此警告视为错误 (`-Werror`)

## 修复方案

将 `[=]` 改为 `[=, this]` 显式捕获 `this`。

**修复后代码**:
```cpp
encoder_callbacks_->dispatcher().post([=, this]() {
  auto buffer = ::Envoy::Buffer::OwnedImpl(body_text_copy);
  encoder_callbacks_->injectEncodedDataToFilterChain(buffer, end_stream);
});
```

## 风险评估

| 风险点 | 级别 | 说明 |
|--------|------|------|
| 功能影响 | 无 | 只是语法修改，不影响功能 |
| 兼容性 | 无 | 显式捕获 `this` 是标准做法 |

## 修复状态

✅ **已修复** - 编译成功
