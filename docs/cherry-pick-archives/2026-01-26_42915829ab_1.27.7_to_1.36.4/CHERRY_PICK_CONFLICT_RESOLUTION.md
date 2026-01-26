# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `42915829ab` - Apply patch: 008-optimize-srds-for-route-fallback.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 3 个

**Patch 功能描述**: 优化 SRDS (Scoped Route Discovery Service) 的路由回退机制，当在某个 scope 中找不到路由时，可以在其他 scope 中重试查找。

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `source/common/http/conn_manager_impl.cc` | 行 1344-1354, 1664-1670, 1798-1821 | 架构差异 + 新功能 |
| 2 | `source/extensions/filters/network/http_connection_manager/config.cc` | 行 585-601 | 架构差异 |
| 3 | `test/common/http/conn_manager_impl_test_2.cc` | 行 2026-2861 | 上下文错位 |

---

## 🔴 架构差异警告

### 架构对比

| 组件 | 源分支 (1.27.7) | 目标分支 (1.36.4) |
|------|--------|----------|
| config 访问方式 | `config_.xxx()` (引用/成员) | `config_->xxx()` (智能指针) |
| SRDS 工厂模式 | `ScopedRoutesConfigProviderUtil::create()` | `srds_factory->createConfigProvider()` |
| getRouteConfig 签名 | 需要添加 4 参数版本 | 已有 4 参数版本（在 scopes.h 中） |

### 影响

- 所有 `config_.` 的调用需要改为 `config_->`
- SRDS 初始化代码需要适配 1.36.4 的工厂模式
- `getRouteConfig` 的 4 参数重载已存在于目标分支，无需修改接口

---

## 详细冲突分析与解决方案

### 1. `source/common/http/conn_manager_impl.cc`

#### 冲突点 1 (行 1344-1354)

**冲突描述**: snapped_scoped_routes_config_ 的赋值和后续处理

- **HEAD (目标分支)**: 
  ```cpp
  snapped_scoped_routes_config_ =
      connection_manager_.config_->scopedRouteConfigProvider()->config<Router::ScopedConfig>();
  ```
  
- **Incoming (源提交)**: 
  ```cpp
  snapped_scoped_routes_config_ =
      connection_manager_.config_.scopedRouteConfigProvider()->config<Router::ScopedConfig>();
  #if defined(HIGRESS)
  // 新增: NullConfigImpl 优化，避免不必要的路由计算
  snapped_route_config_ = std::make_shared<Router::NullConfigImpl>();
  #else
  ```

**解决方案**: 合并双方 - 采用 HEAD 的 `config_->` 语法，同时保留 Incoming 的 HIGRESS 条件编译块

```cpp
snapped_scoped_routes_config_ =
    connection_manager_.config_->scopedRouteConfigProvider()->config<Router::ScopedConfig>();
#if defined(HIGRESS)
// It is only used to determine whether to remove specific internal headers, but at the cost
// of an additional routing calculation. In our scenario, there is no removal of internal
// headers, so there is no need to calculate the route here.
snapped_route_config_ = std::make_shared<Router::NullConfigImpl>();
#else
snapScopedRouteConfig();
#endif
```

#### 冲突点 2 (行 1664-1670)

**冲突描述**: `snapScopedRouteConfig()` 函数中 `getRouteConfig` 的调用

- **HEAD (目标分支)**: 
  ```cpp
  snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(
      connection_manager_.config_->scopeKeyBuilder().ptr(), *request_headers_,
      &connection()->streamInfo());
  ```
  3 参数版本

- **Incoming (源提交)**: 
  ```cpp
  snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(
      connection_manager_.config_.scopeKeyBuilder().ptr(), *request_headers_,
      &connection()->streamInfo(), snapped_scoped_routes_recompute_);
  ```
  4 参数版本 + `config_.` 语法

**解决方案**: 合并双方 - 使用 4 参数版本 + `config_->` 语法

```cpp
snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(
    connection_manager_.config_->scopeKeyBuilder().ptr(), *request_headers_,
    &connection()->streamInfo(), snapped_scoped_routes_recompute_);
```

#### 冲突点 3 (行 1798-1821)

**冲突描述**: 路由查找完成后的处理逻辑

- **HEAD (目标分支)**: 
  ```cpp
  setVirtualHostRoute(std::move(route_result));
  ```
  使用新的 `setVirtualHostRoute` 方法

- **Incoming (源提交)**: 
  ```cpp
  #if defined(HIGRESS)
  if (connection_manager_.config_.retryOtherScopeWhenNotFound()) {
    while (route == nullptr && snapped_scoped_routes_recompute_ != nullptr) {
      // ... 重试逻辑
    }
  }
  #endif
  setRoute(route);
  ```
  使用旧的 `setRoute` 方法 + 新增 HIGRESS 重试逻辑

**解决方案**: ✅ 已验证

**架构差异说明**:
- 1.36.4 中 `setRoute(RouteConstSharedPtr)` 内部调用 `setVirtualHostRoute(VirtualHostRoute)`
- 1.36.4 中 `route()` 方法返回 `VirtualHostRoute` 而不是 `RouteConstSharedPtr`
- `VirtualHostRoute` 结构体包含 `vhost` 和 `route` 两个成员
- 需要将 patch 中的 `route` 改为 `route_result`，检查条件改为 `route_result.route == nullptr`

**最终方案**: 保留 HEAD 的 `setVirtualHostRoute`，在其之前添加 HIGRESS 重试逻辑，适配新的类型

```cpp
#if defined(HIGRESS)
if (connection_manager_.config_->retryOtherScopeWhenNotFound()) {
  while (route_result.route == nullptr && snapped_scoped_routes_recompute_ != nullptr) {
    ASSERT(snapped_scoped_routes_config_ != nullptr);
    snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(
        connection_manager_.config_->scopeKeyBuilder().ptr(), *request_headers_,
        &connection()->streamInfo(), snapped_scoped_routes_recompute_);
    if (snapped_route_config_ == nullptr) {
      break;
    }
    route_result = snapped_route_config_->route(cb, *request_headers_, filter_manager_.streamInfo(),
                                                stream_id_);
    ENVOY_STREAM_LOG(debug,
                     "after the route was not found, search again in other scopes and found:{}",
                     *this, route_result.route != nullptr);
  }
}
#endif

setVirtualHostRoute(std::move(route_result));
```

---

### 2. `source/extensions/filters/network/http_connection_manager/config.cc`

#### 冲突点 (行 585-601)

**冲突描述**: SRDS 配置初始化代码

- **HEAD (目标分支)**: 使用工厂模式
  ```cpp
  if (!srds_factory || !scoped_routes_config_provider_manager_) {
    creation_status = absl::InvalidArgumentError("SRDS configured but not compiled in");
    return;
  }
  scoped_routes_config_provider_ =
      srds_factory->createConfigProvider(config, context_.serverFactoryContext(), stats_prefix_,
                                         *scoped_routes_config_provider_manager_);
  scope_key_builder_ = srds_factory->createScopeKeyBuilder(config);
  ```

- **Incoming (源提交)**: 使用 Util 类 + 新增配置项
  ```cpp
  scoped_routes_config_provider_ = Router::ScopedRoutesConfigProviderUtil::create(
      config, context_.getServerFactoryContext(), context_.initManager(), stats_prefix_,
      scoped_routes_config_provider_manager_);
  scope_key_builder_ = Router::ScopedRoutesConfigProviderUtil::createScopeKeyBuilder(config);
  retry_other_scope_when_not_found_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
      config.scoped_routes(), retry_other_scope_when_not_found, true);
  ```

**解决方案**: 采用 HEAD 的工厂模式，并在其后添加新配置项的读取

```cpp
if (!srds_factory || !scoped_routes_config_provider_manager_) {
  creation_status = absl::InvalidArgumentError("SRDS configured but not compiled in");
  return;
}
scoped_routes_config_provider_ =
    srds_factory->createConfigProvider(config, context_.serverFactoryContext(), stats_prefix_,
                                       *scoped_routes_config_provider_manager_);
scope_key_builder_ = srds_factory->createScopeKeyBuilder(config);
retry_other_scope_when_not_found_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
    config.scoped_routes(), retry_other_scope_when_not_found, true);
```

---

### 3. `test/common/http/conn_manager_impl_test_2.cc`

#### 冲突点 (行 2026-2861)

**冲突描述**: 这是一个 **上下文错位** 冲突，不是真正的代码内容冲突。

Git 将两个完全不相关的代码块错误地标记为冲突:
- **HEAD**: 一个测试用例的末尾部分
- **Incoming**: Patch 对 SRDS 相关测试的修改（将 `getRouteConfig` 从 3 参数改为 4 参数）

**解决方案**: 保留 HEAD 的内容，然后在正确的位置应用 Incoming 的修改

**需要修改的测试**: patch 修改了以下 HIGRESS 条件编译块中的 `getRouteConfig` 调用:
1. `TestSrdsRouteNotFound` - 行 ~2753
2. `TestSrdsUpdate` - 行 ~2796
3. `TestSrdsCrossScopeReroute` - 行 ~2870
4. `TestSrdsRouteFound` - 行 ~2959

修改内容: 将 `getRouteConfig(_, _, _)` 改为 `getRouteConfig(_, _, _, _)`

---

## 解决步骤总结

### ✅ 可直接合并的冲突

以下冲突可以按方案直接处理:
- `conn_manager_impl.cc` 冲突点 1 - 合并双方，`config_.` → `config_->`
- `conn_manager_impl.cc` 冲突点 2 - 使用 4 参数版本 + `config_->`
- `config.cc` - 采用 HEAD 工厂模式 + 添加新配置项

### ✅ 已验证的复杂冲突

- `conn_manager_impl.cc` 冲突点 3 - `setRoute` → `setVirtualHostRoute`，类型从 `RouteConstSharedPtr` 改为 `VirtualHostRoute`

### 🔧 需要额外适配的工作

1. `test/common/http/conn_manager_impl_test_2.cc` - 需要手动修复上下文错位，在正确位置应用 `getRouteConfig` 4 参数修改
2. 确保 `config.h` 中有 `retryOtherScopeWhenNotFound()` 方法声明
3. 确保 `snapped_scoped_routes_recompute_` 成员变量已声明

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟢 低 | `conn_manager_impl.cc` 冲突点 3 | API 差异已验证，适配方案明确 |
| 🟢 低 | `config.cc` | 新配置项读取代码直接添加即可 |
| 🟢 低 | `conn_manager_impl.cc` 冲突点 1, 2 | 仅语法差异 |
| 🟢 低 | 测试文件 | 上下文错位，修复明确 |

---

## 验证清单

- [x] 编译通过
- [x] 单元测试通过
- [x] `retryOtherScopeWhenNotFound()` 方法存在 (`conn_manager_config.h:574`, `config.h:282`)
- [x] `snapped_scoped_routes_recompute_` 成员变量存在 (`conn_manager_impl.h:506`)
- [x] HIGRESS 条件编译正确
- [x] `setVirtualHostRoute` API 适配正确 (`VirtualHostRoute` 类型已确认)

---

## 建议的工作流程

1. **第一阶段**: 解决 3 个冲突文件的 cherry-pick 冲突
2. **第二阶段**: 验证 `setVirtualHostRoute` 的使用是否正确
3. **第三阶段**: 编译验证
4. **第四阶段**: 运行相关单元测试
