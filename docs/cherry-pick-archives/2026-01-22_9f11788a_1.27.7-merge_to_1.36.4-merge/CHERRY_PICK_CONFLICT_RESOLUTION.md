# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `9f11788ab6f2e14fac41db64928ac96dd6c53806` - Apply patch: 000-3-srds.patch
**源分支**: 1.27.7-merge
**目标分支**: 1.36.4-merge
**冲突文件数**: 8 个

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `api/envoy/config/route/v3/route_components.proto` | 行 252, 447, 851 | 新增字段 + 字段编号 |
| 2 | `source/common/http/conn_manager_impl.cc` | 行 813, 1093, 2426 | 新增函数 + 功能代码 |
| 3 | `source/common/router/config_impl.cc` | 行 1703 | 新增静态常量 |
| 4 | `source/common/router/config_impl.h` | 行 449 | 新增静态成员声明 |
| 5 | `source/extensions/filters/network/http_connection_manager/config.cc` | 行 448 | 构造函数代码 |
| 6 | `test/common/http/conn_manager_impl_test_2.cc` | 行 2026 | 测试用例冲突 |
| 7 | `test/common/router/config_impl_test.cc` | 行 12211, 12313 | 测试用例冲突 |
| 8 | `test/common/router/scoped_rds_test.cc` | 行 343 | setup函数冲突 |

---

## 详细冲突分析与解决方案

### 1. `api/envoy/config/route/v3/route_components.proto`

#### 冲突点 1 (行 252-276)
**冲突描述**: VirtualHost message 中新增字段的冲突

- **HEAD (1.36.4-merge)**: 新增了 `metadata` 字段 (field number 24)
- **SRDS patch**: 新增了 `allow_server_names` 字段 (field number 101)

**解决方案**: **保留双方的修改** - 两个字段使用不同的 field number，可以同时存在

```protobuf
  // The metadata field can be used to provide additional information
  // about the virtual host. It can be used for configuration, stats, and logging.
  // The metadata should go under the filter namespace that will need it.
  // For instance, if the metadata is intended for the Router filter,
  // the filter name should be specified as ``envoy.filters.http.router``.
  core.v3.Metadata metadata = 24;

  // If non-empty, a list of server names (such as SNI for the TLS protocol) is used to determine
  // whether this request is allowed to access this VirutalHost. If not allowed, 421 Misdirected Request will be returned.
  //
  // The server name can be matched whith wildcard domains, i.e. ``www.example.com`` can be matched with
  // ``www.example.com``, ``*.example.com`` and ``*.com``.
  //
  // Note that partial wildcards are not supported, and values like ``*w.example.com`` are invalid.
  //
  // This is useful when expose all virtual hosts to arbitrary HCM filters (such as using SRDS), and you want to make
  // mTLS-protected routes invisible to requests with different SNIs.
  //
  // .. attention::
  //
  //   See the :ref:`FAQ entry <faq_how_to_setup_sni>` on how to configure SNI for more
  //   information.
  repeated string allow_server_names = 101;
```

#### 冲突点 2 (行 447-451)
**冲突描述**: WeightedCluster message 的 next-free-field 注释

- **HEAD**: `[#next-free-field: 6]`
- **SRDS patch**: `[#next-free-field: 102]`

**解决方案**: **采用 SRDS patch 版本** - 使用更大的 field number `102`，确保未来扩展空间

```protobuf
// [#next-free-field: 102]
message WeightedCluster {
```

#### 冲突点 3 (行 851-855)
**冲突描述**: RouteAction message 的 next-free-field 注释

- **HEAD**: `[#next-free-field: 43]`
- **SRDS patch**: `[#next-free-field: 1001]`

**解决方案**: **采用 SRDS patch 版本** - 使用更大的 field number `1001`

```protobuf
// [#next-free-field: 1001]
message RouteAction {
```

---

### 2. `source/common/http/conn_manager_impl.cc`

#### 冲突点 1 (行 813-872)
**冲突描述**: RdsRouteConfigUpdateRequester 相关函数

> **⚠️ 重要架构差异发现！**

**1.27 vs 1.36 架构对比:**

| 方面 | 1.27.7-merge | 1.36.4-merge |
|------|-------------|--------------|
| 类位置 | `conn_manager_impl.h` 内嵌类 | `envoy/http/filter.h` 抽象接口 |
| 实现位置 | `conn_manager_impl.cc` | `route_config_update_requster.cc` (独立文件) |
| 类名 | `ConnectionManagerImpl::RdsRouteConfigUpdateRequester` | `RdsRouteConfigUpdateRequester` (独立类) |
| 调用方式 | 直接调用成员函数 | 通过工厂模式创建实例 |

**冲突原因分析:**
- **HEAD (1.36.4-merge)**: 空 - 因为 1.36 架构中这些函数根本不在 `conn_manager_impl.cc` 中
- **SRDS patch (1.27)**: 添加了 HIGRESS 条件编译代码来修改 `computeScopeKey` 的调用方式

**SRDS patch 在 1.27 中的修改内容:**
```cpp
} else if (scope_key_builder_.has_value()) {
#if defined(HIGRESS)
    Router::ScopeKeyPtr scope_key = parent_.snapped_scoped_routes_config_->computeScopeKey(
        scope_key_builder_.ptr(), *parent_.request_headers_, &parent_.connection()->streamInfo());
#else
    Router::ScopeKeyPtr scope_key = scope_key_builder_->computeScopeKey(*parent_.request_headers_);
#endif
```

**解决方案**: **删除冲突块中的 incoming 代码，保留 HEAD (空)**

因为 1.36 的实现在 `source/common/http/route_config_update_requster.cc` 文件中，需要：
1. 在 `conn_manager_impl.cc` 中接受 HEAD 版本（空）
2. **另外修改** `source/common/http/route_config_update_requster.cc` 添加 HIGRESS 条件编译

**需要在 `route_config_update_requster.cc` 中添加的修改:**
```cpp
void RdsRouteConfigUpdateRequester::requestRouteConfigUpdate(
    RouteCache& route_cache, Http::RouteConfigUpdatedCallbackSharedPtr route_config_updated_cb,
    absl::optional<Router::ConfigConstSharedPtr> route_config, Event::Dispatcher& dispatcher,
    RequestHeaderMap& request_headers) {
  if (route_config.has_value() && route_config.value()->usesVhds()) {
    // ... existing code ...
  } else if (scope_key_builder_.has_value()) {
#if defined(HIGRESS)
    // TODO: 需要适配 1.36 架构来获取 snapped_scoped_routes_config_ 和 StreamInfo
    Router::ScopeKeyPtr scope_key = /* 需要适配新接口 */;
#else
    Router::ScopeKeyPtr scope_key = scope_key_builder_->computeScopeKey(request_headers);
#endif
    // ... rest of code ...
  }
}
```

> **注意**: 由于 1.36 的接口签名不同（没有 `parent_` 成员访问），需要重新设计 HIGRESS 的适配方案。这可能需要：
> 1. 修改 `RouteConfigUpdateRequester` 接口增加 StreamInfo 参数
> 2. 或者通过其他方式获取 `snapped_scoped_routes_config_`

#### 冲突点 2 (行 1093-1113)
**冲突描述**: `chargeStats` 函数中的代码差异

- **HEAD**: 新增了 tracing 相关的刷新逻辑 (`trace_refresh_after_route_refresh_`)
- **SRDS patch**: 新增了 HIGRESS 条件编译的 gRPC 状态码转换逻辑

**解决方案**: **合并双方的修改** - 两个功能不冲突，需要同时保留

```cpp
void ConnectionManagerImpl::ActiveStream::chargeStats(const ResponseHeaderMap& headers) {
  if (trace_refresh_after_route_refresh_ && connection_manager_tracing_config_.has_value()) {
    const Tracing::Decision tracing_decision =
        Tracing::TracerUtility::shouldTraceRequest(filter_manager_.streamInfo());
    ConnectionManagerImpl::chargeTracingStats(tracing_decision.reason,
                                              connection_manager_.config_->tracingStats());
  }

  uint64_t response_code = Utility::getResponseStatus(headers);

#if defined(HIGRESS)
  if (Grpc::Common::hasGrpcContentType(headers)) {
    absl::optional<Grpc::Status::GrpcStatus> grpc_status = Grpc::Common::getGrpcStatus(headers);
    if (grpc_status.has_value()) {
      response_code = Grpc::Utility::grpcToHttpStatus(grpc_status.value());
    }
  }
#endif

  filter_manager_.streamInfo().setResponseCode(response_code);
  // ... 后续代码 ...
```

**注意**: 需要删除 SRDS patch 中重复的 `uint64_t response_code = Utility::getResponseStatus(headers);` 行（行1115），因为已在合并代码中定义。

#### 冲突点 3 (行 2426-2437)
**冲突描述**: `recreateStream` 函数的实现差异

- **HEAD**: 只有一行 `ENVOY_EXECUTION_SCOPE(trackedStream(), active_span_.get());`
- **SRDS patch**: 在 HIGRESS 条件下新增了重载函数和 `use_original_request_body` 参数

**解决方案**: **合并双方的修改** - 保留 HEAD 的 execution scope 调用，并添加 SRDS 的函数重载

```cpp
#if defined(HIGRESS)
void ConnectionManagerImpl::ActiveStream::recreateStream(
    StreamInfo::FilterStateSharedPtr filter_state) {
  ENVOY_EXECUTION_SCOPE(trackedStream(), active_span_.get());
  return recreateStream(filter_state, false);
}
void ConnectionManagerImpl::ActiveStream::recreateStream(
    StreamInfo::FilterStateSharedPtr filter_state, bool use_original_request_body) {
#else
void ConnectionManagerImpl::ActiveStream::recreateStream(
    StreamInfo::FilterStateSharedPtr filter_state) {
  ENVOY_EXECUTION_SCOPE(trackedStream(), active_span_.get());
#endif
  ResponseEncoder* response_encoder = response_encoder_;
  // ... 后续代码 ...
```

---

### 3. `source/common/router/config_impl.cc`

#### 冲突点 (行 1703-1722)
**冲突描述**: 新增静态常量定义

- **HEAD**: 空
- **SRDS patch**: 新增多个静态常量:
  - `VirtualHostImpl::SSL_REDIRECT_ROUTE`
  - `SslPermanentRedirectRoute::SSL_PERMANENT_REDIRECTOR` (HIGRESS)
  - `VirtualHostImpl::SSL_PERMANENT_REDIRECT_ROUTE` (HIGRESS)
  - `SNIRedirectRoute::SNI_REDIRECTOR` (HIGRESS)
  - `VirtualHostImpl::SNI_REDIRECT_ROUTE` (HIGRESS)

**解决方案**: **采用 SRDS patch 版本** - 这些是 SRDS 功能所需的静态常量

```cpp
const std::shared_ptr<const SslRedirectRoute> VirtualHostImpl::SSL_REDIRECT_ROUTE{
    new SslRedirectRoute()};

#if defined(HIGRESS)
const SslPermanentRedirector SslPermanentRedirectRoute::SSL_PERMANENT_REDIRECTOR;
const std::shared_ptr<const SslPermanentRedirectRoute>
    VirtualHostImpl::SSL_PERMANENT_REDIRECT_ROUTE{new SslPermanentRedirectRoute};

const SNIRedirector SNIRedirectRoute::SNI_REDIRECTOR;
const envoy::config::core::v3::Metadata SNIRedirectRoute::metadata_;
const Envoy::Config::TypedMetadataImpl<Envoy::Config::TypedMetadataFactory>
    SNIRedirectRoute::typed_metadata_({});

const std::shared_ptr<const SNIRedirectRoute> VirtualHostImpl::SNI_REDIRECT_ROUTE{
    new SNIRedirectRoute()};
#endif
```

---

### 4. `source/common/router/config_impl.h`

#### 冲突点 (行 449-457)
**冲突描述**: VirtualHostImpl 类的静态成员声明

- **HEAD**: 空
- **SRDS patch**: 新增静态成员声明

**解决方案**: **采用 SRDS patch 版本**

```cpp
  static const std::shared_ptr<const SslRedirectRoute> SSL_REDIRECT_ROUTE;
#if defined(HIGRESS)
  static const std::shared_ptr<const SslPermanentRedirectRoute> SSL_PERMANENT_REDIRECT_ROUTE;
  static const std::shared_ptr<const SNIRedirectRoute> SNI_REDIRECT_ROUTE;
#endif
```

---

### 5. `source/extensions/filters/network/http_connection_manager/config.cc`

#### 冲突点 (行 448-463)
**冲突描述**: HttpConnectionManagerConfig 构造函数中的初始化代码

> **⚠️ 判断已修正** (Phase 4 深度验证后)

- **HEAD (1.36.4-merge)**: 包含 `local_reply_` 和 `http2_options_` 的初始化逻辑 (带错误处理)
- **SRDS patch (1.27)**: 添加了 HIGRESS 条件编译块，新增 `keepalive_header_timeout_` 成员初始化

**架构对比**:

| 方面 | 1.27 (SRDS patch) | 1.36.4-merge |
|------|-------------------|--------------|
| 构造函数初始化列表 | 添加 `keepalive_header_timeout_` (HIGRESS) | 无此成员 |
| 构造函数体开始 | `if (!idle_timeout_)` | 先做 `creation_status` 检查 |

**解决方案**: **合并双方** - 保留 HIGRESS 的新成员初始化 + 保留 1.36 的错误处理逻辑

```cpp
      add_proxy_protocol_connection_state_(
#if defined(HIGRESS)
          PROTOBUF_GET_WRAPPED_OR_DEFAULT(config, add_proxy_protocol_connection_state, true)),
      keepalive_header_timeout_(PROTOBUF_GET_SECONDS_OR_DEFAULT(config, keepalive_header_timeout,
                                                                KeepaliveHeaderTimeoutSeconds)) {
#else
          PROTOBUF_GET_WRAPPED_OR_DEFAULT(config, add_proxy_protocol_connection_state, true)) {
#endif
  if (!creation_status.ok()) {
    return;
  }
  auto local_reply_or_error = LocalReply::Factory::create(config.local_reply_config(), context);
  SET_AND_RETURN_IF_NOT_OK(local_reply_or_error.status(), creation_status);
  local_reply_ = std::move(*local_reply_or_error);

  auto options_or_error = Http2::Utility::initializeAndValidateOptions(
      config.http2_protocol_options(), config.has_stream_error_on_invalid_http_message(),
      config.stream_error_on_invalid_http_message());
  SET_AND_RETURN_IF_NOT_OK(options_or_error.status(), creation_status);
  http2_options_ = options_or_error.value();
```

> **依赖说明**: `config.h` 头文件中的相关修改（成员声明、getter 函数、静态常量）在 cherry-pick 中已自动合并成功，无需额外处理。

---

### 6. `test/common/http/conn_manager_impl_test_2.cc`

#### 冲突点 (行 2026-2859)
**冲突描述**: 测试文件结构严重分歧 (约 833 行冲突)

> **⚠️ 深度分析结果**

**架构差异**:

| 方面 | 1.27.7-merge | 1.36.4-merge |
|------|--------------|--------------|
| 行 2020+ 附近内容 | 编解码器测试 + SRDS 测试 | `DownstreamRemoteResetConnectError` 测试 |
| SRDS 测试用例 | ✅ 存在 (行 2750+) | ❌ 不存在 |
| 文件行数 | 约 3000+ 行 | 约 2100+ 行 |

**冲突原因**:
1. **1.27.7-merge**: 在行 2750+ 已有 SRDS 测试用例 (`TestSrdsRouteNotFound`, `TestSrdsUpdate`, `TestSrdsCrossScopeReroute`, `TestSrdsRouteFound`)
2. **SRDS patch**: 为这些已存在的测试添加 HIGRESS 条件编译块
3. **1.36.4-merge**: 完全没有这些 SRDS 测试用例，文件结构不同
4. **冲突**: Git 无法对齐两个分支的上下文，产生 800+ 行冲突

**SRDS patch 实际修改内容**:
- 为 `TestSrdsRouteNotFound` 添加 HIGRESS 条件（使用 `getRouteConfig(_, _, _)` 代替 `computeScopeKey`）
- 为 `TestSrdsUpdate` 添加 HIGRESS 条件
- 为 `TestSrdsCrossScopeReroute` 添加 HIGRESS 条件
- 为 `TestSrdsRouteFound` 添加 HIGRESS 条件

**解决方案**: **分步处理**

1. **保留 HEAD (1.36) 的测试内容** - `DownstreamRemoteResetConnectError` 等测试
2. **从 1.27 分支提取完整的 SRDS 测试用例** - 包含 HIGRESS 条件编译的版本
3. **将 SRDS 测试添加到 1.36 文件末尾** - 作为新增测试用例

**具体操作步骤**:
```bash
# 1. 先采用 HEAD 版本解决冲突
git checkout --ours test/common/http/conn_manager_impl_test_2.cc
git add test/common/http/conn_manager_impl_test_2.cc

# 2. 从 1.27 分支提取 SRDS 测试代码
git show 9f11788ab6:test/common/http/conn_manager_impl_test_2.cc | \
  sed -n '/TEST_F(HttpConnectionManagerImplTest, TestSrdsRouteNotFound)/,/^TEST_F.*NewConnection/p' \
  > /tmp/srds_tests.cc

# 3. 手动将 SRDS 测试添加到 1.36 文件的适当位置
```

> **注意**: 需要确保 SRDS 测试所依赖的 mock 类和辅助函数在 1.36 中可用。可能需要同步 `conn_manager_impl_test_base.h` 中的相关定义。

---

### 7. `test/common/router/config_impl_test.cc`

#### 冲突点 1 (行 12211-12243)
**冲突描述**: SslRedirectRoute 测试断言差异

- **HEAD**: 包含对 `SslRedirectRoute` 属性的详细断言测试
- **SRDS patch**: 包含 HIGRESS 条件下的 redirect response code 测试

**解决方案**: **合并双方** - 两个测试互补

```cpp
  EXPECT_NE(nullptr, dynamic_cast<const SslRedirectRoute*>(accepted_route.get()));

  {
    EXPECT_EQ(nullptr, accepted_route->routeEntry());
    EXPECT_EQ(nullptr, accepted_route->decorator());
    EXPECT_EQ(nullptr, accepted_route->tracingConfig());
    EXPECT_EQ(nullptr, accepted_route->mostSpecificPerFilterConfig("any"));
    EXPECT_EQ(absl::nullopt, accepted_route->filterDisabled("any"));
    EXPECT_TRUE(accepted_route->perFilterConfigs("any").empty());

    accepted_route->metadata();
    accepted_route->typedMetadata();
    accepted_route->routeName();
  }

#if defined(HIGRESS)
  EXPECT_EQ(Http::Code::MovedPermanently,
            dynamic_cast<const SslRedirectRoute*>(accepted_route.get())
                ->directResponseEntry()
                ->responseCode());
  RouteConstSharedPtr accepted_route_post = config.route(
      [](RouteConstSharedPtr, RouteEvalStatus) -> RouteMatchStatus {
        ADD_FAILURE() << "RouteCallback should not be invoked since there are no matching "
                         "route to override";
        return RouteMatchStatus::Continue;
      },
      genHeaders("bat.com", "/", "POST"));
  EXPECT_EQ(Http::Code::PermanentRedirect,
            dynamic_cast<const SslRedirectRoute*>(accepted_route_post.get())
                ->directResponseEntry()
                ->responseCode());
#endif
```

#### 冲突点 2 (行 12313-12606)
**冲突描述**: 新增测试用例冲突

- **HEAD**: 包含 `RequestMirrorPoliciesWithTraceSampled` 和 `RequestBodyBufferLimitPrecedence` 测试
- **SRDS patch**: 包含多个 HIGRESS 条件下的 AllowServerNames 相关测试

**解决方案**: **保留双方** - 两组测试不冲突

```cpp
TEST_F(RouteMatcherTest, RequestMirrorPoliciesWithTraceSampled) {
  // ... HEAD 的测试代码 ...
}

TEST_F(RouteConfigurationV2, RequestBodyBufferLimitPrecedenceRouteOverridesVirtualHost) {
  // ... HEAD 的测试代码 ...
}

#if defined(HIGRESS)
TEST_F(RouteMatchOverrideTest, NullRouteOnExactAllowServerNames) {
  // ... SRDS patch 的测试代码 ...
}

TEST_F(RouteMatchOverrideTest, NullRouteOnWildcardAllowServerNames) {
  // ... SRDS patch 的测试代码 ...
}

TEST_F(RouteMatchOverrideTest, NullRouteOnEmptyAllowServerNames) {
  // ... SRDS patch 的测试代码 ...
}

TEST_F(RouteMatchOverrideTest, NullRouteOnAllowServerNamesWithoutSsl) {
  // ... SRDS patch 的测试代码 ...
}
#endif
```

---

### 8. `test/common/router/scoped_rds_test.cc`

#### 冲突点 (行 343-419)
**冲突描述**: ScopedRdsTest 类的 setup 函数

- **HEAD**: 简单的 `void setup()` 函数声明
- **SRDS patch**: 新增 `setupHostScope()` 函数 (HIGRESS) 和修改后的 `setup()` 函数签名

**解决方案**: **采用 SRDS patch 版本** - 保留新增的 `setupHostScope()` 函数和更新的 `setup()` 签名

```cpp
class ScopedRdsTest : public ScopedRoutesTestBase {
protected:
#if defined(HIGRESS)
  void setupHostScope(const OptionalHttpFilters optional_http_filters = OptionalHttpFilters()) {
    // ... 完整实现 ...
  }
#endif
  void setup(const OptionalHttpFilters optional_http_filters = OptionalHttpFilters()) {
    // ... 实现 ...
  }
```

---

## 🔴 重要架构差异警告

**本次 cherry-pick 面临的核心挑战不是简单的代码合并，而是 1.27 和 1.36 之间存在显著的架构差异！**

### 架构对比

| 组件 | 1.27.7-merge | 1.36.4-merge |
|------|-------------|--------------|
| RouteConfigUpdateRequester | 内嵌类 `ConnectionManagerImpl::RdsRouteConfigUpdateRequester` | 独立接口 `RouteConfigUpdateRequester` (在 `envoy/http/filter.h`) |
| 实现文件 | `conn_manager_impl.cc` | `route_config_update_requster.cc` (独立文件) |
| 创建方式 | 直接实例化 | 工厂模式 `RouteConfigUpdateRequesterFactory` |
| 函数签名 | 使用 `parent_` 成员访问 | 通过参数传递 `RouteCache&` |

### 影响

这意味着 SRDS patch 中对 `requestRouteConfigUpdate()` 的 HIGRESS 修改**不能直接迁移**，需要重新适配到 1.36 的架构。

---

## 解决步骤总结

### ✅ 可直接合并的冲突
以下文件可以采用 SRDS patch 版本或简单合并:
- `source/common/router/config_impl.cc` - 采用 SRDS patch
- `source/common/router/config_impl.h` - 采用 SRDS patch
- `api/envoy/config/route/v3/route_components.proto` - 合并双方字段

### ⚠️ 需要手动合并
以下文件需要仔细手动合并:
1. `source/common/http/conn_manager_impl.cc` - 冲突点2、3需合并，冲突点1保留HEAD
2. `source/extensions/filters/network/http_connection_manager/config.cc` - **合并双方** (已修正：保留HIGRESS的keepalive_header_timeout_ + 保留1.36的错误处理)
3. `test/common/http/conn_manager_impl_test_2.cc` - 复杂合并
4. `test/common/router/config_impl_test.cc` - 合并双方测试用例
5. `test/common/router/scoped_rds_test.cc` - 采用 SRDS patch

### 🔧 需要额外适配的工作
由于架构差异，以下工作需要在 cherry-pick 之外完成:
1. **修改 `source/common/http/route_config_update_requster.cc`** - 将 HIGRESS 条件编译适配到 1.36 的接口
2. **可能需要修改接口** - 如果需要访问 `snapped_scoped_routes_config_` 和 `StreamInfo`

---

## 执行命令

```bash
# 1. 解决冲突后标记文件
git add <resolved_file>

# 2. 完成所有冲突解决后继续 cherry-pick
git cherry-pick --continue

# 3. 如需放弃
git cherry-pick --abort
```

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🔴 高 | `conn_manager_impl.cc` 冲突点1 | **架构差异**，1.27 的实现不适用于 1.36 |
| 🔴 高 | `route_config_update_requster.cc` | 需要额外适配 HIGRESS 代码 (非冲突文件) |
| 🟡 中 | `conn_manager_impl.cc` 冲突点2、3 | 功能代码合并，需要仔细测试 |
| 🟡 中 | `http_connection_manager/config.cc` | 合并双方：HIGRESS keepalive + 1.36 错误处理 (已验证) |
| 🟢 低 | proto 文件 | 只是添加字段，向后兼容 |
| 🟢 低 | 测试文件 | 主要是测试用例的合并 |

---

## 验证清单

- [ ] 编译通过
- [ ] Proto 文件生成正确
- [ ] 单元测试通过
- [ ] HIGRESS 条件编译正确
- [ ] SRDS 功能正常工作
- [ ] `route_config_update_requster.cc` 中的 HIGRESS 适配完成
- [ ] On-demand SRDS 更新功能验证

---

## 建议的工作流程

1. **第一阶段**: 解决 cherry-pick 冲突，完成基础合并
2. **第二阶段**: 适配 `route_config_update_requster.cc` 中的 HIGRESS 代码
3. **第三阶段**: 修改相关接口（如需要）
4. **第四阶段**: 全面测试 SRDS 功能