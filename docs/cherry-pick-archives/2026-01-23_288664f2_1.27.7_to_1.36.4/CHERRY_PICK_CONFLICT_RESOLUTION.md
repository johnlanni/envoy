# Cherry-pick 冲突解决方案

## 概述

| 项目 | 值 |
|------|-----|
| **原始提交** | `288664f2a9` |
| **提交信息** | Apply patch: 000-5-internal-redirect-policy.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **冲突文件数** | 9 |
| **冲突块数** | 14 |

## 架构差异说明

### 关键发现

1. **DynamicRouteEntry/WeightedClusterEntry 重构**
   - **1.27.7**: 这两个类是 `RouteEntryImplBase` 的内部类，直接实现所有 RouteEntry 接口
   - **1.36.4**: `DynamicRouteEntry` 已移至 `delegating_route_impl.h`，继承自 `DelegatingRouteEntry` 委托类
   - **影响**: Patch 中关于 `DynamicRouteEntry` 和 `WeightedClusterEntry` 的代码**不需要合并**，1.36.4 通过委托模式自动处理

2. **async_client_impl.h 重构**
   - **1.27.7**: 包含 `NullHedgePolicy`, `RouteEntryImpl`, `RouteImpl` 等结构（~640 行）
   - **1.36.4**: 这些结构已被移除或重构（仅 ~402 行）
   - **影响**: Patch 中向 `async_client_impl.h` 添加的代码**不需要合并**

3. **目标分支已有 internalActiveRedirectPolicy 支持**
   - `delegating_route_impl.h` 第 136-140 行已有 `#if defined(HIGRESS)` 的 `internalActiveRedirectPolicy()` 方法

## 冲突文件列表

| # | 文件 | 冲突块数 | 冲突类型 | 解决策略 |
|---|------|---------|---------|---------|
| 1 | `envoy/router/BUILD` | 1 | 依赖添加 | ✅ 采用 Incoming |
| 2 | `envoy/router/router.h` | 1 | 接口添加 | ✅ 采用 Incoming |
| 3 | `source/common/http/async_client_impl.h` | 1 | 大块代码 | ⚠️ 采用 HEAD（架构已重构） |
| 4 | `source/common/router/BUILD` | 1 | 依赖添加 | ✅ 采用 Incoming |
| 5 | `source/common/router/config_impl.cc` | 1 | 初始化列表 | 🔧 需手动合并 |
| 6 | `source/common/router/config_impl.h` | 2 | 类定义 | ⚠️ 采用 HEAD（架构已重构） |
| 7 | `test/common/router/config_impl_test.cc` | 3 | 测试用例 | 🔧 需手动合并 |
| 8 | `test/common/router/router_test.cc` | 2 | 测试用例 | 🔧 需手动合并 |
| 9 | `test/common/router/router_test_base.h` | 1 | 辅助函数 | 🔧 需手动合并 |

---

## 详细冲突分析

### 冲突 1: `envoy/router/BUILD` (行 79-85)

**冲突内容**:
```
<<<<<<< HEAD
=======
    higress_deps = [
        "//contrib/envoy/http:active_redirect_policy_interface",
    ],
    external_deps = ["abseil_optional"],
>>>>>>> 288664f2a9
```

**分析**: 
- HEAD: 空（无 higress_deps）
- Incoming: 添加 `active_redirect_policy_interface` 依赖

**解决方案**: ✅ **采用 Incoming** - 添加 HIGRESS 依赖

---

### 冲突 2: `envoy/router/router.h` (行 1163-1173)

**冲突内容**:
```cpp
<<<<<<< HEAD
=======
   * @return std::string& the name of the route.
   */
  virtual const std::string& routeName() const PURE;

#if defined(HIGRESS)
  virtual const InternalActiveRedirectPolicy& internalActiveRedirectPolicy() const PURE;
#endif
  /**
>>>>>>> 288664f2a9
```

**分析**: 
- HEAD: 无 `routeName()` 和 `internalActiveRedirectPolicy()` 声明
- Incoming: 添加这两个方法声明

**解决方案**: ✅ **采用 Incoming** - 添加接口声明

---

### 冲突 3: `source/common/http/async_client_impl.h` (行 174-414)

**冲突内容**: 大块代码，包含 `NullHedgePolicy`, `NullRateLimitPolicy`, `NullCommonConfig`, `NullVirtualHost`, `NullPathMatchCriterion`, `RouteEntryImpl`, `RouteImpl` 等结构。

**分析**:
- **架构差异**: 1.36.4 中这些结构已移至 `null_route_impl.h`（第 40 行已 include 该文件）
- HEAD: 无这些结构（已移至 null_route_impl.h）
- Incoming: 添加完整的内部类实现

**解决方案**: ⚠️ **采用 HEAD**

**⚠️ 后续操作**: 需要在 `source/common/http/null_route_impl.h` 的 `RouteEntryImpl` 类中添加 HIGRESS 代码：
```cpp
#if defined(HIGRESS)
  const Router::InternalActiveRedirectPolicy& internalActiveRedirectPolicy() const override {
    return internal_active_redirect_policy_;
  }
#endif
```
以及对应的静态成员变量。

---

### 冲突 4: `source/common/router/BUILD` (行 38-44)

**冲突内容**:
```
<<<<<<< HEAD
=======
    higress_deps = [
        "//contrib/common/active_redirect/source:active_redirect_policy_lib",
    ],
    external_deps = ["abseil_optional"],
>>>>>>> 288664f2a9
```

**分析**: 
- HEAD: 空
- Incoming: 添加 `active_redirect_policy_lib` 依赖

**解决方案**: ✅ **采用 Incoming** - 添加 HIGRESS 依赖

---

### 冲突 5: `source/common/router/config_impl.cc` (行 461-484)

**冲突内容**:
```cpp
<<<<<<< HEAD
      route_tracing_(parseRouteTracing(route)), route_name_(route.name()),
      time_source_(factory_context.mainThreadDispatcher().timeSource()),
      per_request_buffer_limit_(PROTOBUF_GET_WRAPPED_OR_DEFAULT(
          route, per_request_buffer_limit_bytes, std::numeric_limits<uint32_t>::max())),
      request_body_buffer_limit_(PROTOBUF_GET_WRAPPED_OR_DEFAULT(route, request_body_buffer_limit,
                                                                 vhost->requestBodyBufferLimit())),
=======
      route_tracing_(parseRouteTracing(route)),
      direct_response_body_(...),
      per_filter_configs_(...),
#if !defined(HIGRESS)
      route_name_(route.name()), time_source_(...),
#else
      route_name_(route.name()), time_source_(...),
      internal_active_redirect_policy_(
          buildActiveInternalRedirectPolicy(route.route(), validator, route.name())),
#endif
      retry_shadow_buffer_limit_(...),
>>>>>>> 288664f2a9
```

**分析**: 
- 这是构造函数初始化列表
- HEAD 版本有 `per_request_buffer_limit_` 和 `request_body_buffer_limit_`
- Incoming 版本有 `direct_response_body_`, `per_filter_configs_`, `internal_active_redirect_policy_`
- 两个版本字段不完全一致

**解决方案**: 🔧 **需手动合并** - 保留 HEAD 的字段结构，添加 HIGRESS 条件编译的 `internal_active_redirect_policy_` 初始化

---

### 冲突 6: `source/common/router/config_impl.h` (行 855-1132, 1234-1255)

**冲突内容**: 
- 块1 (855-1132): `DynamicRouteEntry` 和 `WeightedClusterEntry` 类定义
- 块2 (1234-1255): 其他代码

**分析**:
- **架构差异**: 1.36.4 中 `DynamicRouteEntry` 已移至 `delegating_route_impl.h`
- `delegating_route_impl.h` 第 136-140 行**已有** `internalActiveRedirectPolicy()` 方法
- Incoming 的 `DynamicRouteEntry` 和 `WeightedClusterEntry` 代码**不适用于** 1.36.4 架构

**解决方案**: ⚠️ **采用 HEAD** - 不需要合并这些类，1.36.4 通过委托模式已处理

---

### 冲突 7: `test/common/router/config_impl_test.cc` (行 11243-11603)

**冲突内容**: 测试用例
- 块1 (11243-11248): 测试函数名冲突
- 块2 (11256-11293): YAML 配置差异
- 块3 (11301-11603): 大量 HIGRESS 测试代码

**分析**:
- HEAD 有 `InternalRedirectPolicyAcceptsResponseHeadersToPrserve` 等测试
- Incoming 有 `InternalActiveRedirectIsDisabledWhenNotSpecifiedInRouteAction` 等 HIGRESS 测试

**解决方案**: 🔧 **需手动合并**
- 保留 HEAD 的测试
- 在 `#if defined(HIGRESS)` 块中添加 Incoming 的 HIGRESS 测试
- 注意：Incoming 使用 `google_re2: {}` 而 HEAD 使用简写形式

---

### 冲突 8: `test/common/router/router_test.cc` (行 5207-5490)

**冲突内容**: 测试用例
- 块1 (5207-5369): HEAD 有 `ResponseHeadersTCopyCopiesHeadersOrClears` 测试，Incoming 有 HIGRESS 测试
- 块2 (5379-5490): 更多 HIGRESS 测试

**分析**:
- HEAD 有标准 internal redirect 测试
- Incoming 有 active redirect 相关测试

**解决方案**: 🔧 **需手动合并**
- 保留 HEAD 的测试
- 在 `#if defined(HIGRESS)` 块中添加 Incoming 的 HIGRESS 测试

---

### 冲突 9: `test/common/router/router_test_base.h` (行 111-120)

**冲突内容**:
```cpp
<<<<<<< HEAD
  // Recreates filter under test after any values that affect its constructor were changed.
  void recreateFilter();
=======
#if defined(HIGRESS)
  void enableActiveRedirects(std::string redirect_url, uint32_t max_internal_redirects = 1,
                             bool forced_use_original_host = false,
                             bool forced_add_header_before_route_matcher = false);
  void setNumPreviousActiveRedirect(uint32_t num_previous_redirects);
>>>>>>> 288664f2a9
```

**分析**: 
- HEAD: 有 `recreateFilter()` 函数
- Incoming: 有 `enableActiveRedirects()` 和 `setNumPreviousActiveRedirect()` 函数

**解决方案**: 🔧 **需手动合并** - 两边都需要保留

---

## 风险评估

| 风险项 | 等级 | 说明 |
|--------|------|------|
| 架构不兼容 | 中 | `async_client_impl.h` 和 `config_impl.h` 中的类已重构 |
| 接口签名差异 | 低 | 目标分支已有 `internalActiveRedirectPolicy()` 接口 |
| 测试适配 | 中 | 测试代码需要适配 1.36.4 的 API |

## 验证清单

- [x] 编译通过：`bazel build --config=clang //source/common/router:config_lib //source/common/router:router_lib //source/common/http:null_route_impl_lib`
- [x] 单元测试通过：`bazel test --config=clang //test/common/router:config_impl_test`
- [x] 单元测试通过：`bazel test --config=clang //test/common/router:router_test`
