# 编译修复计划

## 概述

Cherry-pick 000-4 patch 后编译遇到以下问题并已修复。

## 修复记录

### 问题 1: Proto 文件字段重复定义 (预存在问题)

**文件**: `api/envoy/config/route/v3/route_components.proto`

**错误信息**:
```
"cluster_specifier_plugin" is already defined in "envoy.config.route.v3.WeightedCluster".
"inline_cluster_specifier_plugin" is already defined in "envoy.config.route.v3.WeightedCluster".
Field number 100 has already been used...
Field number 101 has already been used...
```

**原因**: 在 `WeightedCluster` message 中，字段 `cluster_specifier_plugin` (field 100) 和 `inline_cluster_specifier_plugin` (field 101) 被重复定义了两次。

**修复**: 删除重复的字段定义（行 592-600）。

---

### 问题 2: API 变化 - getByKey → get

**文件**: `source/extensions/http/custom_response/redirect_policy/redirect_policy.cc`

**错误信息**:
```
error: no member named 'getByKey' in 'Envoy::Http::RequestHeaderMap'
```

**原因**: 1.36 版本中 `RequestHeaderMap` 的 `getByKey` 方法已被 `get` 方法替代。

**修复**:
```cpp
// 原代码 (1.27)
const auto x_envoy_original_host = downstream_headers->getByKey(
    ::Envoy::Http::CustomHeaders::get().AliExtendedValues.XEnvoyOriginalHost);
if (x_envoy_original_host && !(*x_envoy_original_host).empty()) {
  real_original_host = *x_envoy_original_host;
}

// 修复后 (1.36)
const auto x_envoy_original_host_result = downstream_headers->get(
    ::Envoy::Http::CustomHeaders::get().AliExtendedValues.XEnvoyOriginalHost);
if (!x_envoy_original_host_result.empty() &&
    !x_envoy_original_host_result[0]->value().empty()) {
  real_original_host = std::string(x_envoy_original_host_result[0]->value().getStringView());
}
```

---

### 问题 3: 缺少头文件包含

**文件**: `source/extensions/filters/http/custom_response/custom_response_filter.cc`

**错误信息**:
```
fatal error: 'source/common/grpc/common.h' file not found
```

**原因**: HIGRESS 代码使用了 `Grpc::Common::isGrpcRequestHeaders`，但缺少对应的头文件包含。

**修复**: 
1. 添加头文件包含:
   ```cpp
   #include "source/common/grpc/common.h"
   ```
2. 更新 BUILD 文件，添加依赖:
   ```python
   "//source/common/grpc:common_lib",
   ```

---

## 编译验证结果

| 目标 | 状态 |
|------|------|
| `//source/common/http:filter_manager_lib` | ✅ 成功 |
| `//source/common/router:router_lib` | ✅ 成功 |
| `//source/extensions/filters/http/custom_response:config` | ✅ 成功 |
| `//source/extensions/http/custom_response/redirect_policy:redirect_policy_lib` | ✅ 成功 |

## 修改的文件汇总

| 文件 | 修改类型 |
|------|---------|
| `api/envoy/config/route/v3/route_components.proto` | 删除重复字段定义 |
| `source/extensions/http/custom_response/redirect_policy/redirect_policy.cc` | API 适配 |
| `source/extensions/filters/http/custom_response/custom_response_filter.cc` | 添加头文件 |
| `source/extensions/filters/http/custom_response/BUILD` | 添加依赖 |
