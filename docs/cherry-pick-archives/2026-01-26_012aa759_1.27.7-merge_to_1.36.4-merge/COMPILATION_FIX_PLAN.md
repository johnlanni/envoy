# 编译修复计划 - Cherry-pick 012aa759

## 基本信息

- **提交**: 012aa7596e13ea3301fba10a3fb532bbfaa8b064
- **源分支**: 1.27.7-merge
- **目标分支**: 1.36.4-merge
- **日期**: 2026-01-26

## 编译错误分析

### 错误 1: UDPA 依赖仓库变更 ✅ 已修复

**错误信息**:
```
ERROR: no such package '@@com_github_cncf_udpa//udpa/annotations': 
The repository '@@com_github_cncf_udpa' could not be resolved: 
Repository '@@com_github_cncf_udpa' is not defined
```

**根因分析**:
- 在 Envoy 1.27.7 中，UDPA 注解使用 `@com_github_cncf_udpa//udpa/annotations:pkg`
- 在 Envoy 1.36.4 中，UDPA 注解已迁移到 `@com_github_cncf_xds//udpa/annotations:pkg`
- Cherry-pick 的 proto BUILD 文件使用了旧的依赖路径

**受影响文件**:
1. `api/contrib/envoy/extensions/filters/http/mcp_sse_stateful_session/v3alpha/BUILD`
2. `api/contrib/envoy/extensions/http/mcp_sse_stateful_session/envelope/v3alpha/BUILD`

### 错误 2: load_balancer_lib 目标不存在 ✅ 已修复

**错误信息**:
```
ERROR: no such target '//source/common/upstream:load_balancer_lib': 
target 'load_balancer_lib' not declared in package 'source/common/upstream'
```

**根因分析**:
- 在 1.27.7 中存在 `//source/common/upstream:load_balancer_lib` 目标
- 在 1.36.4 中该目标已不存在，只有 `//envoy/upstream:load_balancer_interface`
- 源代码只需要 interface，不需要 lib

**受影响文件**:
1. `contrib/mcp_sse_stateful_session/filters/http/source/BUILD`

## 已执行的修复

### 修复 1: 更新 proto BUILD 文件依赖 ✅

**文件 1**: `api/contrib/envoy/extensions/filters/http/mcp_sse_stateful_session/v3alpha/BUILD`
- 将 `@com_github_cncf_udpa//udpa/annotations:pkg` 改为 `@com_github_cncf_xds//udpa/annotations:pkg`

**文件 2**: `api/contrib/envoy/extensions/http/mcp_sse_stateful_session/envelope/v3alpha/BUILD`
- 将 `@com_github_cncf_udpa//udpa/annotations:pkg` 改为 `@com_github_cncf_xds//udpa/annotations:pkg`

### 修复 2: 移除不存在的依赖 ✅

**文件**: `contrib/mcp_sse_stateful_session/filters/http/source/BUILD`
- 移除 `//source/common/upstream:load_balancer_lib` 依赖

## 验证命令

```bash
bazel build --config=clang //contrib/mcp_sse_stateful_session/...
```

### 错误 3: QueryParams API 变更 🔧 待修复

**错误信息**:
```
contrib/mcp_sse_stateful_session/http/source/envelope.cc:136:23: error: 
no member named 'parseQueryString' in namespace 'Envoy::Http::Utility'
```

**根因分析**:
- 在 1.27.7 中: `Envoy::Http::Utility::parseQueryString()` 返回 `QueryParams`（map-like）
- 在 1.36.4 中: 改为 `Envoy::Http::Utility::QueryParamsMulti::parseQueryString()`
- `QueryParamsMulti` 类接口不同：
  - 没有 `find()`, `end()` 方法
  - 使用 `getFirstValue(key)` 获取单个值
  - 使用 `data()` 获取底层 btree_map 进行迭代

**受影响文件**:
- `contrib/mcp_sse_stateful_session/http/source/envelope.cc`

### 错误 4: Factory Config API 变更 ✅ 已修复

**问题**:
- `createRouteSpecificFilterConfigTyped` 返回类型从 `Router::RouteSpecificFilterConfigConstSharedPtr` 改为 `absl::StatusOr<Router::RouteSpecificFilterConfigConstSharedPtr>`
- 构造函数需要 `CommonFactoryContext` 而非 `FactoryContext`

**受影响文件**:
- `contrib/mcp_sse_stateful_session/filters/http/source/config.h`
- `contrib/mcp_sse_stateful_session/filters/http/source/config.cc`
- `contrib/mcp_sse_stateful_session/filters/http/test/stateful_session_test.cc`

### 错误 5: HIGRESS admin.cc API 变更 ✅ 已修复

**问题**:
- `getByKey()` 方法已改为 `get()`
- 返回值检查从 `if (value)` 改为 `if (!value.empty())`

**受影响文件**:
- `source/server/admin/admin.cc`

## 状态

- [x] 修复 1: proto BUILD 文件依赖
- [x] 修复 2: 移除不存在的 load_balancer_lib
- [x] 修复 3: QueryParams API 变更
- [x] 修复 4: Factory Config API 变更
- [x] 修复 5: HIGRESS admin.cc API 变更
- [x] **编译验证通过！**
- [x] **单元测试通过！** (3/3 tests pass)
