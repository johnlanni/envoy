# 单元测试修复计划

## 概述

Cherry-pick 000-4 patch 后单元测试编译遇到以下问题并已修复。

## 修复记录

### 问题 1: API 变化 - getByKey → get (测试文件)

**文件**: `test/extensions/filters/http/custom_response/custom_response_filter_test.cc`

**错误信息**:
```
error: no member named 'getByKey' in 'Envoy::Http::TestRequestHeaderMapImpl'
  335 |   EXPECT_EQ("x-bar2", request_headers.getByKey("foo2"));
```

**原因**: 1.36 版本中 `TestRequestHeaderMapImpl` 的 `getByKey` 方法已被 `get` 方法替代（与源代码中相同的 API 变化）。

**修复**:
```cpp
// 原代码 (1.27)
EXPECT_EQ("x-bar2", request_headers.getByKey("foo2"));

// 修复后 (1.36)
{
  auto result = request_headers.get(::Envoy::Http::LowerCaseString("foo2"));
  ASSERT_FALSE(result.empty());
  EXPECT_EQ("x-bar2", result[0]->value().getStringView());
}
```

---

## 测试目标

| 测试目标 | 状态 |
|---------|------|
| `//test/common/http:filter_manager_test` | 待验证 |
| `//test/extensions/filters/http/custom_response:custom_response_filter_test` | 编译修复完成，待运行 |

## 修改的文件汇总

| 文件 | 修改类型 |
|------|---------|
| `test/extensions/filters/http/custom_response/custom_response_filter_test.cc` | API 适配 (getByKey → get) |

## 验证命令

```bash
bazel build --config=clang //test/common/http:filter_manager_test //test/extensions/filters/http/custom_response:custom_response_filter_test

bazel test --config=clang //test/common/http:filter_manager_test //test/extensions/filters/http/custom_response:custom_response_filter_test
```
