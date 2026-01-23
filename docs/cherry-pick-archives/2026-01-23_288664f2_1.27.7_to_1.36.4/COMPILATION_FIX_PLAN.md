# 编译修复计划

## 概述

Cherry-pick 000-5-internal-redirect-policy.patch 后，由于 1.27.7 和 1.36.4 之间的 API 差异，需要进行以下编译修复。

## 编译错误列表

### 错误 1: `abseil_optional` 依赖不存在

**错误信息**:
```
ERROR: ... no such target '//external:abseil_optional': target 'abseil_optional' not declared in package 'external'
```

**原因**: 1.36.4 中 `abseil_optional` 不再作为 `external_deps` 存在

**修复方案**:
```diff
# contrib/common/active_redirect/source/BUILD
- external_deps = ["abseil_optional"],
```

---

### 错误 2: `parseRegex` 函数签名变化

**错误信息**:
```
error: no matching function for call to 'parseRegex'
error: no matching constructor for initialization of 'const Regex::CompiledMatcherPtr'
```

**原因**: 1.36.4 中 `Regex::Utility::parseRegex()` 需要 `regex_engine` 参数，且返回 `absl::StatusOr<CompiledMatcherPtr>`

**修复方案**:
```diff
# contrib/common/active_redirect/source/active_redirect_policy_impl.h
- InternalActiveRedirectPolicyImpl(...);
+ InternalActiveRedirectPolicyImpl(..., Regex::Engine& regex_engine);

- InternalActiveRedirectPoliciesImpl(...);
+ InternalActiveRedirectPoliciesImpl(..., Regex::Engine& regex_engine);
```

```diff
# contrib/common/active_redirect/source/active_redirect_policy_impl.cc
- Regex::Utility::parseRegex(redirect_policy.response_header_pattern().pattern(), ...);
+ THROW_OR_RETURN_VALUE(
+     Regex::Utility::parseRegex(redirect_policy.response_header_pattern().pattern(), regex_engine),
+     Regex::CompiledMatcherPtr);
```

---

### 错误 3: `translateOpaqueConfig` 返回值变化

**错误信息**:
```
error: ignoring return value of function declared with 'nodiscard' attribute
```

**原因**: 1.36.4 中 `Envoy::Config::Utility::translateOpaqueConfig()` 返回 `absl::Status`

**修复方案**:
```diff
# contrib/common/active_redirect/source/active_redirect_policy_impl.cc
- Envoy::Config::Utility::translateOpaqueConfig(...);
+ THROW_IF_NOT_OK(Envoy::Config::Utility::translateOpaqueConfig(...));
```

---

### 错误 4: `HeaderParser::configure` 返回值变化

**错误信息**:
```
error: no viable conversion from 'absl::StatusOr<std::unique_ptr<HeaderParser>>' to 'std::unique_ptr<HeaderParser>'
```

**原因**: 1.36.4 中 `HeaderParser::configure()` 返回 `absl::StatusOr<HeaderParserPtr>`

**修复方案**:
```diff
# contrib/common/active_redirect/source/active_redirect_policy_impl.cc
- request_headers_parser_ = HeaderParser::configure(...);
+ request_headers_parser_ = THROW_OR_RETURN_VALUE(HeaderParser::configure(...), HeaderParserPtr);
```

---

### 错误 5: `DynamicRouteEntry` 未声明

**错误信息**:
```
error: use of undeclared identifier 'DynamicRouteEntry'
```

**原因**: 1.36.4 中 `DynamicRouteEntry` 已移至 `delegating_route_impl.h`，且 API 不兼容

**修复方案**: 移除使用 `DynamicRouteEntry` 的 `clone()` 方法

```diff
# source/common/router/config_impl.h
  #if defined(HIGRESS)
    std::unique_ptr<InternalActiveRedirectPoliciesImpl>
    buildActiveInternalRedirectPolicy(...) const;
-   
-   std::unique_ptr<DynamicRouteEntry> clone() const;
  #endif
```

---

### 错误 6: `PathUtil` 命名空间未声明

**错误信息**:
```
error: no member named 'PathUtil' in namespace 'Envoy::Http'
```

**原因**: 缺少 `path_utility.h` 头文件

**修复方案**:
```diff
# source/common/router/router.cc
+ #include "source/common/http/path_utility.h"
```

---

### 错误 7: `routeName()` 不在 `RouteEntry`/`DirectResponseEntry` 中

**错误信息**:
```
error: no member named 'routeName' in 'Envoy::Router::DirectResponseEntry'
error: no member named 'routeName' in 'Envoy::Router::RouteEntry'
```

**原因**: 1.36.4 中 `routeName()` 是 `Route` 接口的方法，不是 `RouteEntry` 或 `DirectResponseEntry` 的方法

**修复方案**:
```diff
# source/common/router/router.cc
- route->directResponseEntry()->routeName()
+ route->routeName()

- route->routeEntry()->routeName()
+ route->routeName()
```

---

### 错误 8: `AsyncStreamImpl::RouteEntryImpl` 不存在

**错误信息**:
```
error: no member named 'RouteEntryImpl' in 'Envoy::Http::AsyncStreamImpl'
```

**原因**: 1.36.4 中 `RouteEntryImpl` 已移至 `null_route_impl.h`

**修复方案**:
```diff
# source/common/http/async_client_impl.cc
- #if defined(HIGRESS)
- const Router::InternalActiveRedirectPoliciesImpl
-     AsyncStreamImpl::RouteEntryImpl::internal_active_redirect_policy_;
- #endif
```

静态成员已在 `null_route_impl.cc` 中定义。

---

### 错误 9: `MockRoute` 缺少 `internalActiveRedirectPolicy()` 实现

**错误信息**:
```
error: allocating an object of abstract class type 'NiceMock<MockRoute>'
note: unimplemented pure virtual method 'internalActiveRedirectPolicy' in 'NiceMock'
```

**原因**: `RouteEntry` 接口声明了 `internalActiveRedirectPolicy()` 为 PURE virtual，但 `MockRoute` 未实现

**修复方案**:
```diff
# test/mocks/router/mocks.h
  MOCK_METHOD(const ConnectConfigOptRef, connectConfig, (), (const));
+ #if defined(HIGRESS)
+   MOCK_METHOD(const InternalActiveRedirectPolicy&, internalActiveRedirectPolicy, (), (const));
+ #endif
  MOCK_METHOD(const UpgradeMap&, upgradeMap, (), (const));

  // 添加成员变量
+ #if defined(HIGRESS)
+   testing::NiceMock<MockInternalActiveRedirectPolicy> internal_active_redirect_policy_;
+ #endif
```

```diff
# test/mocks/router/mocks.cc
MockRoute::MockRoute() {
  ...
+ #if defined(HIGRESS)
+   ON_CALL(*this, internalActiveRedirectPolicy())
+       .WillByDefault(ReturnRef(internal_active_redirect_policy_));
+ #endif
}
```

## 验证命令

```bash
# 编译验证
bazel build --config=clang //source/common/router:config_lib //source/common/router:router_lib //source/common/http:null_route_impl_lib

# 单元测试验证
bazel test --config=clang //test/common/router:config_impl_test //test/common/router:router_test
```

## 测试结果

```
Executed 2 out of 2 tests: 2 tests pass.
```
