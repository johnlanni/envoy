# 测试问题修复方案文档

## 修复状态

| 阶段 | 测试套件 | 状态 |
|------|---------|------|
| Phase 1 | `//test/common/http:conn_manager_impl_test` | ✅ 已修复 |
| Phase 1 | `//test/common/router:config_impl_test` | ✅ PASSED |
| Phase 1 | `//test/common/router:scoped_config_impl_test` | ✅ PASSED |
| Phase 1 | `//test/extensions/filters/network/http_connection_manager:config_test` | ✅ PASSED |
| **Phase 2** | `//test/common/router:scoped_rds_test` | ❌ **编译失败 - 待修复** |

---

## Phase 2: scoped_rds_test 编译问题 (已修复 ✅)

### 问题概览

| 问题类型 | 数量 | 状态 |
|---------|------|------|
| `OptionalHttpFilters` 类型未定义 | 4 处 | ✅ 已修复 |
| `nodiscard` 属性警告 | 13+ 处 | ✅ 已修复 |
| `ProtobufWkt` 类型未定义 | 2 处 | ✅ 已修复 |
| `createStaticConfigProvider` 参数类型不匹配 | 1 处 | ✅ 已修复 |

---

### 架构差异分析

#### 问题 1: `OptionalHttpFilters` 类型差异

| 项目 | 1.27 版本 | 1.36 版本 |
|------|-----------|-----------|
| **类型定义** | `using OptionalHttpFilters = absl::flat_hash_set<std::string>;` | ❌ **已移除** |
| **定义位置** | `source/common/router/config_impl.h:84` | 不存在 |
| **用途** | 显式传递可选过滤器名称集合 | 改用 proto 中的 `is_optional` 字段 |

**1.27 中的 `ScopedRoutesConfigProviderManagerOptArg` 构造函数**:
```cpp
ScopedRoutesConfigProviderManagerOptArg(
    std::string scoped_routes_name,
    const envoy::config::core::v3::ConfigSource& rds_config_source,
    const OptionalHttpFilters& optional_http_filters)  // ← 已在 1.36 中移除
```

**1.36 中的 `ScopedRoutesConfigProviderManagerOptArg` 构造函数**:
```cpp
ScopedRoutesConfigProviderManagerOptArg(
    std::string scoped_routes_name,
    const envoy::config::core::v3::ConfigSource& rds_config_source)  // ← 只有 2 个参数
```

**错误位置**:
- `scoped_rds_test.cc:343` - `setupHostScope()` 函数参数
- `scoped_rds_test.cc:414` - `setup()` 函数参数  
- `scoped_rds_test.cc:659` - 变量声明
- `scoped_rds_test.cc:1895` - 函数调用

#### 问题 2: `nodiscard` 属性差异

| 项目 | 1.27 版本 | 1.36 版本 |
|------|-----------|-----------|
| **`onConfigUpdate` 返回类型** | `void` 或 `absl::Status` | `absl::Status` (带 `ABSL_MUST_USE_RESULT`) |
| **忽略返回值** | ✅ 允许 | ❌ 编译错误 |

**错误位置**: `scoped_rds_test.cc:496, 524, 570, 590, 634, 645, 675, 716, 756, 802, 843, 886, 928` 等

---

### 修复方案

#### 方案 A: 移除 `OptionalHttpFilters` 参数 (推荐)

由于 1.36 架构中 `OptionalHttpFilters` 已被移除，需要：

1. **移除 `OptionalHttpFilters` 参数**:
   ```cpp
   // 修改前 (1.27 风格)
   void setup(const OptionalHttpFilters optional_http_filters = OptionalHttpFilters()) {
       ...
       ScopedRoutesConfigProviderManagerOptArg(name, rds_config_source, optional_http_filters)
   }
   
   // 修改后 (1.36 风格)
   void setup() {
       ...
       ScopedRoutesConfigProviderManagerOptArg(name, rds_config_source)
   }
   ```

2. **修复 `nodiscard` 警告**:
   ```cpp
   // 修改前
   EXPECT_THROW_WITH_REGEX(
       srds_subscription_->onConfigUpdate(decoded_resources.refvec_, "1"),
       EnvoyException, "...");
   
   // 修改后 - 使用 EXPECT_THAT 检查 Status
   EXPECT_THAT(
       srds_subscription_->onConfigUpdate(decoded_resources.refvec_, "1").message(),
       testing::ContainsRegex("..."));
   ```
   
   或者使用 lambda 包装：
   ```cpp
   EXPECT_THROW_WITH_REGEX(
       [&]() { 
         auto status = srds_subscription_->onConfigUpdate(decoded_resources.refvec_, "1");
         if (!status.ok()) throw EnvoyException(std::string(status.message()));
       }(),
       EnvoyException, "...");
   ```

#### 方案 B: 删除 HIGRESS 特定测试 (低工作量)

如果 HIGRESS 特定测试不是必需的，可以直接删除 `#if defined(HIGRESS)` 块。

---

### 详细修改清单 (方案 A - 推荐)

| 文件 | 行号 | 修改内容 |
|-----|------|---------|
| `scoped_rds_test.cc` | 343 | `setupHostScope()` 移除 `OptionalHttpFilters` 参数 |
| `scoped_rds_test.cc` | 406-410 | 更新 `ScopedRoutesConfigProviderManagerOptArg` 构造调用 |
| `scoped_rds_test.cc` | 414 | `setup()` 移除 `OptionalHttpFilters` 参数 |
| `scoped_rds_test.cc` | 480-484 | 更新 `ScopedRoutesConfigProviderManagerOptArg` 构造调用 |
| `scoped_rds_test.cc` | 659 | 删除 `OptionalHttpFilters` 变量声明 |
| `scoped_rds_test.cc` | 730-732 | 删除 `optional_http_filters.insert()` 调用，更新 `setup()` 调用 |
| `scoped_rds_test.cc` | 1963-1966 | 更新 `ScopedRoutesConfigProviderManagerOptArg` 构造调用 |
| `scoped_rds_test.cc` | 多处 | 修复 `onConfigUpdate` 的 `nodiscard` 警告 |

### 详细修改清单 (方案 B - 删除 HIGRESS 测试)

| 文件 | 行号 | 修改内容 |
|-----|------|---------|
| `scoped_rds_test.cc` | 342-412 | 删除整个 `#if defined(HIGRESS)` 块（`setupHostScope` 函数）|

---

### ✅ 修复执行记录 (方案 A)

**修复时间**: 2026-01-22

**执行的修改**:

1. **移除 `OptionalHttpFilters` 参数**:
   - `setupHostScope()` 函数签名改为无参数
   - `setup()` 函数签名改为无参数
   - `ScopedRoutesConfigProviderManagerOptArg` 构造函数调用改为 2 参数版本

2. **删除废弃测试**:
   - 删除 `OptionalUnknownFactoryForPerVirtualHostTypedConfig` 测试（该测试依赖已废弃的 `OptionalHttpFilters` 功能）

3. **修复 `ProtobufWkt` 类型**:
   - 将 `ProtobufWkt::Any` 替换为 `Protobuf::Any`

4. **修复 `nodiscard` 警告**:
   - `EXPECT_NO_THROW(srds_subscription_->onConfigUpdate(...))` 改为 `EXPECT_TRUE(srds_subscription_->onConfigUpdate(...).ok())`
   - `EXPECT_THROW_WITH_MESSAGE/REGEX` 改为使用 `status` 变量检查

5. **修复 `createStaticConfigProvider` 参数类型**:
   - 将单个 `ScopedRouteConfiguration` 包装为 `ProtobufTypes::ConstMessagePtrVector`

**测试结果**: `Executed 1 out of 1 test: 1 test passes.`

---

## Phase 1: conn_manager_impl_test 问题 (已修复)

### ✅ 修复完成

**修复时间**: 2026-01-22

**测试结果**: `Executed 1 out of 1 test: 1 test passes.`

---

## 测试失败概览 (Phase 1 - 修复前)

| 测试套件 | 失败测试数 | 通过测试数 | 状态 |
|---------|-----------|-----------|------|
| `//test/common/http:conn_manager_impl_test` | 4 → 0 | 大部分 → 全部 | ❌ → ✅ PASSED |
| `//test/common/router:config_impl_test` | 0 | 全部 | ✅ PASSED |
| `//test/common/router:scoped_config_impl_test` | 0 | 全部 | ✅ PASSED |
| `//test/extensions/filters/network/http_connection_manager:config_test` | 0 | 全部 | ✅ PASSED |

---

## 失败的测试用例详情

### 1. `HttpConnectionManagerImplTest.TestSrdsCrossScopeReroute` (shard 1)

**位置**: `test/common/http/conn_manager_impl_test_3.cc:1338`

**失败原因**:
```
EXPECT_CALL(*static_cast<const Router::MockScopedConfig*>(
                scopedRouteConfigProvider()->config<Router::ScopedConfig>().get()),
            getRouteConfig(_))
    .Times(3)
    // Expected: to be called 3 times
    // Actual: never called - unsatisfied and active
```

### 2. `HttpConnectionManagerImplTest.TestSrdsRouteFound` (shard 2)

**位置**: `test/common/http/conn_manager_impl_test_3.cc:1424`

**失败原因**:
```
EXPECT_CALL(*scopedRouteConfigProvider()->config<Router::MockScopedConfig>(), getRouteConfig(_))
    .Times(2)
    // Expected: to be called twice
    // Actual: never called - unsatisfied and active
```

### 3. `HttpConnectionManagerImplTest.TestSrdsRouteNotFound` (shard 4)

**失败原因**: 同上，`getRouteConfig(_)` mock 未被调用

### 4. `HttpConnectionManagerImplTest.TestSrdsUpdate` (shard 5)

**失败原因**: 同上，`getRouteConfig(_)` mock 未被调用

---

## 根因分析

### 问题根源

所有失败的测试都与 **SRDS (Scoped Route Discovery Service)** 相关。问题出在 HIGRESS 条件编译下，`snapScopedRouteConfig()` 调用了一个**不同签名**的 `getRouteConfig` 方法：

#### 非 HIGRESS 模式 (测试预期)
```cpp
// conn_manager_impl.cc:1650-1652
auto scope_key = connection_manager_.config_->scopeKeyBuilder()->computeScopeKey(*request_headers_);
snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(scope_key);
```
调用签名: `getRouteConfig(const ScopeKeyPtr&)`

#### HIGRESS 模式 (实际执行)
```cpp
// conn_manager_impl.cc:1644-1646
snapped_route_config_ = snapped_scoped_routes_config_->getRouteConfig(
    connection_manager_.config_->scopeKeyBuilder().ptr(), *request_headers_,
    &connection()->streamInfo());
```
调用签名: `getRouteConfig(const ScopeKeyBuilder*, const Http::HeaderMap&, const StreamInfo::StreamInfo*)`

### Mock 类定义

`test/mocks/router/mocks.h:667-681` 中的 `MockScopedConfig` 正确定义了两个版本的 mock：

```cpp
class MockScopedConfig : public ScopedConfig {
public:
  MOCK_METHOD(ConfigConstSharedPtr, getRouteConfig, (const ScopeKeyPtr& scope_key), (const));

#if defined(HIGRESS)
  MOCK_METHOD(ConfigConstSharedPtr, getRouteConfig,
              (const ScopeKeyBuilder*, const Http::HeaderMap&, const StreamInfo::StreamInfo*),
              (const));
#endif
};
```

`test/mocks/router/mocks.cc:199-204` 也正确设置了默认行为：

```cpp
MockScopedConfig::MockScopedConfig() {
  ON_CALL(*this, getRouteConfig(_)).WillByDefault(Return(route_config_));
#if defined(HIGRESS)
  ON_CALL(*this, getRouteConfig(_, _, _)).WillByDefault(Return(route_config_));
#endif
}
```

### 测试代码问题

测试代码中只设置了单参数版本的 `EXPECT_CALL`：

```cpp
EXPECT_CALL(*scopedRouteConfigProvider()->config<Router::MockScopedConfig>(), getRouteConfig(_))
    .Times(2);
```

在 HIGRESS 模式下，这个 mock 永远不会被调用，因为实际调用的是三参数版本。

---

## 修复方案

### 方案 A: 条件编译测试 mock 设置 (推荐)

在 SRDS 相关测试中，根据 HIGRESS 条件编译选择正确的 mock 签名。

**修改位置**: `test/common/http/conn_manager_impl_test_3.cc`

**修改示例** (以 `TestSrdsRouteFound` 为例):

```cpp
// 原代码 (conn_manager_impl_test_3.cc:1437-1440)
EXPECT_CALL(*scopedRouteConfigProvider()->config<Router::MockScopedConfig>(), getRouteConfig(_))
    .Times(2);

// 修改为:
#if defined(HIGRESS)
  EXPECT_CALL(*scopedRouteConfigProvider()->config<Router::MockScopedConfig>(), 
              getRouteConfig(_, _, _))
      .Times(2);
#else
  EXPECT_CALL(*scopedRouteConfigProvider()->config<Router::MockScopedConfig>(), getRouteConfig(_))
      .Times(2);
#endif
```

**需要修改的测试用例**:
1. `TestSrdsCrossScopeReroute` (line 1362-1375)
2. `TestSrdsRouteFound` (line 1437-1440)
3. `TestSrdsRouteNotFound` (待定位)
4. `TestSrdsUpdate` (待定位)

### 方案 B: 使用通用的 `_` 匹配器

如果 mock 框架支持，可以使用更通用的匹配器来匹配任意参数数量。但 GoogleMock 不支持这种方式，因为这是两个不同的重载方法。

**结论**: 方案 A 是唯一可行的方案。

---

## 详细修改清单

### 文件 1: `test/common/http/conn_manager_impl_test_3.cc`

| 行号 | 测试用例 | 修改内容 |
|-----|---------|---------|
| 1244-1248 | `TestSrdsRouteNotFound` | 将 `getRouteConfig(_)` mock 改为条件编译版本 |
| 1279-1285 | `TestSrdsUpdate` | 将 `getRouteConfig(_)` mock 改为条件编译版本 |
| 1362-1375 | `TestSrdsCrossScopeReroute` | 将 `getRouteConfig(_)` mock 改为条件编译版本 |
| 1437-1440 | `TestSrdsRouteFound` | 将 `getRouteConfig(_)` mock 改为条件编译版本 |

### 可能需要同步修改

- `computeScopeKey` 的 mock 也可能需要条件编译调整（HIGRESS 版本接受额外的 `StreamInfo*` 参数）

**注意**: 在 HIGRESS 模式下，`getRouteConfig` 的三参数版本内部会调用 `computeScopeKey`，因此 `computeScopeKey` 的 mock 设置可能也需要更新。但从当前 mock 实现来看，`MockScopeKeyBuilder` 已经为两种模式提供了不同的 mock 方法定义。

---

## 修复步骤

1. **定位所有受影响的测试**: 搜索 `conn_manager_impl_test_3.cc` 中所有使用 `getRouteConfig(_)` 的 SRDS 测试

2. **应用条件编译**: 为每个受影响的 `EXPECT_CALL` 添加 `#if defined(HIGRESS)` 条件分支

3. **处理返回值逻辑**: 确保 HIGRESS 版本的 mock 返回相同的路由配置

4. **同步修改 `computeScopeKey`**: 如果需要，同时修改 `computeScopeKey` 的 mock 设置

5. **重新编译并测试**: 
   ```bash
   bazel test --config=clang -c opt //test/common/http:conn_manager_impl_test
   ```

---

## 风险评估

| 风险项 | 级别 | 说明 |
|-------|------|------|
| 测试覆盖遗漏 | 低 | 修改后两种模式都有相应的测试覆盖 |
| 条件编译遗漏 | 中 | 可能有其他未发现的 SRDS 测试需要同样修改 |
| Mock 行为不一致 | 低 | 两个版本的 mock 默认行为已经一致 |

---

## 待确认事项

### Phase 2 (scoped_rds_test)

1. **选择修复方案**: A (删除 HIGRESS 测试) / B (完整适配) / C (暂时跳过)
2. **确认 `OptionalHttpFilters` 的正确定义**: 如果选择方案 B，需要确定正确的类型定义
3. **确认修复优先级**: 是立即修复还是记录为已知问题

### Phase 1 (已完成)

1. ~~确认是否还有其他 SRDS 相关测试需要修改~~ → 已在 Phase 2 发现 `scoped_rds_test`
2. ~~确认 `computeScopeKey` 是否需要同步修改~~ → 不需要
3. ~~确认修复优先级~~ → 已修复

---

请确认 **Phase 2** 的修复方案后，我将开始执行修复操作。
