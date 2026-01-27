# 测试修复计划

## 概述

Cherry-pick `0c0fc31187` (017-misc-opt-of-wasm-rebuild-mem-and-pb-cache.patch) 后，测试阶段遇到以下问题需要修复。

## 测试修复列表

### 1. stream_info_impl_test.cc

**测试目标**: `//test/common/stream_info:stream_info_impl_test`

**问题描述**:
`assertStreamInfoSize()` 断言中的 sizeof 值列表需要更新，以覆盖不同编译配置下的结构体大小。

**修复内容**:
```cpp
// 合并 HEAD 和 Incoming 的 sizeof 值
ASSERT_TRUE(
    sizeof(stream_info) == 728 ||  // docker-msan
    sizeof(stream_info) == 736 ||  // docker-clang
    sizeof(stream_info) == 704 ||  // docker-clang-libc++
    sizeof(stream_info) == 840 || sizeof(stream_info) == 856 ||
    sizeof(stream_info) == 888 || sizeof(stream_info) == 776 ||
#if defined(HIGRESS)
    sizeof(stream_info) == 816 || sizeof(stream_info) == 768 ||
    sizeof(stream_info) == 784 ||  // protobuf hash cache
#endif
    sizeof(stream_info) == 744)
```

**修复文件**:
- `test/common/stream_info/stream_info_impl_test.cc`

### 2. wasm_test.cc

**测试目标**: `//test/extensions/common/wasm:wasm_test`

**问题描述**:
测试文件需要添加对 `rebuild(bool)` 方法的测试覆盖。

**修复内容**:
- 添加 rebuild 功能的单元测试
- 验证 is_fail_recovery 参数的正确行为
- 验证 rebuild_total 和 recover_total 计数器

**修复文件**:
- `test/extensions/common/wasm/wasm_test.cc`

## 测试结果

### 被修改的测试文件

| 测试文件 | 修改类型 | 状态 |
|----------|----------|------|
| `test/common/stream_info/stream_info_impl_test.cc` | sizeof 断言更新 | ✅ 通过 |
| `test/extensions/common/wasm/wasm_test.cc` | 新增测试用例 | ✅ 通过 |
| `test/common/http/conn_manager_impl_fuzz_test.cc` | 自动合并 | ✅ 通过 |
| `test/common/http/conn_manager_impl_test_base.cc` | 自动合并 | ✅ 通过 |
| `test/common/protobuf/utility_test.cc` | 自动合并 | ✅ 通过 |
| `test/extensions/filters/http/wasm/wasm_filter_test.cc` | 自动合并 | ✅ 通过 |
| `test/extensions/filters/http/wasm/test_data/test_cpp.cc` | 自动合并 | ✅ 通过 |
| `test/test_common/wasm_base.h` | 自动合并 | ✅ 通过 |

### 相关测试执行结果

```bash
# 编译测试目标
bazel build --config=clang \
  //test/common/stream_info:stream_info_impl_test \
  //test/extensions/common/wasm:wasm_test
# 结果: 通过

# 运行测试
bazel test --config=clang \
  //test/common/stream_info:stream_info_impl_test \
  //test/extensions/common/wasm:wasm_test
# 结果: 通过
```

## 修复统计

| 修复类型 | 文件数 | 说明 |
|----------|--------|------|
| sizeof 断言更新 | 1 | 合并多配置的结构体大小 |
| 新增测试用例 | 1 | rebuild 功能测试 |
