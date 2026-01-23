# 编译修复计划

## 编译错误概览

| 序号 | 错误类型 | 位置 | 描述 |
|------|---------|------|------|
| 1 | 目标不存在 | `source/extensions/filters/network/common/redis/BUILD:118` | 引用了不存在的 `//:examples_library` 目标 |
| 2 | 目标不存在 | `source/extensions/filters/network/common/redis/BUILD:102` | 引用了不存在的 `//:examples_library` 目标 |
| 3 | 目标不存在 | `source/extensions/common/redis/BUILD:23` | 引用了不存在的 `//:examples_library` 目标 |
| 4 | 目标路径变更 | `source/common/redis/BUILD:22` | `load_balancer_lib` 从 `source/common/upstream` 移到了 `source/extensions/load_balancing_policies/common` |
| 5 | 接口变更 | `source/common/redis/async_client_impl.cc:148` | `HostImpl` 构造函数签名变化：1.36.4 使用工厂方法 `HostImpl::create()`，且参数列表发生变化（新增 `locality_metadata`，移除 `timeSource`） |
| 6 | 接口变更 | `source/common/redis/async_client_impl.cc:75` | `LoadBalancer::chooseHost()` 返回类型从 `HostConstSharedPtr` 变为 `HostSelectionResponse`，需要从 `response.host` 获取 host |

## 错误分析

### 问题 1: `//:examples_library` 目标不存在

**错误信息**:
```
ERROR: no such target '//:examples_library': target 'examples_library' not declared in package ''
```

**根因分析**:
- 1.27.7 分支的根 `BUILD` 文件定义了 `examples_library` 目标
- 1.36.4 分支的根 `BUILD` 文件**没有**定义 `examples_library` 目标
- Cherry-pick 的代码在 `raw_client_lib` 和 `raw_client_interface` 的 visibility 中引用了 `examples_library`

**目标分支可用的 visibility 目标**:
- `//:extension_library` ✅ 存在
- `//:contrib_library` ✅ 存在
- `//:examples_library` ❌ 不存在

## 修复方案

### 方案：移除 `examples_library` 引用

从 visibility 列表中移除 `//:examples_library`，保留其他有效的引用。

**需要修改的文件**:
- `source/extensions/filters/network/common/redis/BUILD`

**修改内容**:

1. `raw_client_interface` (行 102-116):
```bazel
# 修改前
visibility = [
    "//:contrib_library",
    "//:examples_library",   # ← 移除
    "//:extension_library",
    "//envoy/redis:__pkg__",
],

# 修改后
visibility = [
    "//:contrib_library",
    "//:extension_library",
    "//envoy/redis:__pkg__",
],
```

2. `raw_client_lib` (行 118-137):
```bazel
# 修改前
visibility = [
    "//:contrib_library",
    "//:examples_library",   # ← 移除
    "//:extension_library",
    "//source/common/redis:__pkg__",
],

# 修改后
visibility = [
    "//:contrib_library",
    "//:extension_library",
    "//source/common/redis:__pkg__",
],
```

## 风险评估

| 风险等级 | 说明 |
|----------|------|
| 🟢 低 | 仅修改 visibility 配置，不影响实际代码逻辑 |

## 验证计划

修复后重新编译验证：
```bash
bazel build --config=clang -c opt \
  //source/common/redis:async_client_lib \
  //source/extensions/filters/network/common/redis:raw_client_lib
```
