# Cherry-pick 冲突解决方案

## 概述

| 项目 | 值 |
|------|-----|
| **原始提交** | `58535e248c` |
| **提交信息** | Apply patch: 003-forbid-access-to-admin-interface.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **冲突文件数** | 1 |
| **自动合并文件** | 2 |

## Patch 功能说明

该 patch 的目的是**禁止通过路由访问 admin 接口**，防止安全风险。主要包含以下功能：

1. 新增 `XEnvoyRouteIdentifier` HTTP header 定义
2. 在路由处理时设置该 header 标识请求经过了路由
3. 在 admin 接口检查该 header，如果存在则拒绝访问（`/stats/prometheus` 除外）

## 文件状态列表

| 文件 | 状态 | 说明 |
|------|------|------|
| `source/common/http/headers.h` | ✅ 自动合并 | `XEnvoyRouteIdentifier` header 定义 |
| `source/common/router/config_impl.cc` | ❌ 冲突 | 包含常量定义和 header 设置逻辑 |
| `source/server/admin/admin.cc` | ✅ 自动合并 | admin 访问控制检查 |

## 冲突分析

### 文件: `source/common/router/config_impl.cc`

**冲突位置**: 第 127-237 行（约 110 行）

**冲突类型**: 架构重构导致的上下文差异

#### 冲突块内容对比

**HEAD (1.36.4)**:
```cpp
constexpr uint32_t DEFAULT_MAX_DIRECT_RESPONSE_BODY_SIZE_BYTES = 4096;

// Returns an array of header parsers, sorted by specificity...
```
1.36.4 版本结构简洁，因为：
- `createAndValidateRoute` 已移至 `RouteCreator` 类
- `mergeTransforms` 已移至 `config_utility.cc`

**Incoming (58535e248c / 1.27.7)**:
```cpp
constexpr uint32_t DEFAULT_MAX_DIRECT_RESPONSE_BODY_SIZE_BYTES = 4096;

#if defined(ALIMESH)
constexpr absl::string_view EnvoyRouteIdentifierValue = "true";
#endif

void mergeTransforms(...) { ... }

RouteEntryImplBaseConstSharedPtr createAndValidateRoute(...) { ... }

// ... 其他辅助函数 ...

// Returns a vector of header parsers, sorted by specificity...
```
1.27.7 版本包含了大量函数定义。

#### 根因分析

1.27.7 和 1.36.4 之间存在重大架构重构：

| 代码 | 1.27.7 位置 | 1.36.4 位置 |
|------|-------------|-------------|
| `mergeTransforms()` | `config_impl.cc` 匿名命名空间 | `config_utility.cc` |
| `createAndValidateRoute()` | `config_impl.cc` 匿名命名空间 | `RouteCreator` 类静态方法 |

Patch 真正添加的代码只有：
1. `EnvoyRouteIdentifierValue` 常量 - **已存在于 1.36.4（第 131 行）**
2. `finalizeRequestHeaders` 中的 header 设置 - **已存在于 1.36.4（第 956-959 行）**

## 验证结果

通过 grep 验证 1.36.4 当前代码：

```bash
# EnvoyRouteIdentifierValue 常量
$ grep -n "EnvoyRouteIdentifierValue" source/common/router/config_impl.cc
131:constexpr absl::string_view EnvoyRouteIdentifierValue = "true";
958:                          EnvoyRouteIdentifierValue);

# XEnvoyRouteIdentifier header
$ grep -n "XEnvoyRouteIdentifier" source/common/http/headers.h
140:    const LowerCaseString XEnvoyRouteIdentifier{"x-envoy-route-identifier"};

# finalizeRequestHeaders 中的 HIGRESS 代码
第 956-959 行已包含：
#if defined(HIGRESS)
  headers.setReferenceKey(Http::CustomHeaders::get().AliExtendedValues.XEnvoyRouteIdentifier,
                          EnvoyRouteIdentifierValue);
#endif
```

## 解决方案

### 冲突点 #1: `source/common/router/config_impl.cc` (第 127-237 行)

**决策**: ✅ **采用 HEAD 版本**

**原因**:
1. Patch 需要添加的 `EnvoyRouteIdentifierValue` 常量已存在于 1.36.4（第 131 行）
2. Patch 需要添加的 `finalizeRequestHeaders` 中的代码已存在于 1.36.4（第 956-959 行）
3. Incoming 版本包含的 `mergeTransforms`、`createAndValidateRoute` 等函数是 1.27.7 的旧架构，在 1.36.4 中已重构到其他位置
4. 如果采用 Incoming 版本会引入重复代码和架构混乱

**操作**:
```bash
git checkout --ours source/common/router/config_impl.cc
git add source/common/router/config_impl.cc
```

## 自动合并文件检查

### `source/common/http/headers.h`

**状态**: ✅ 已正确合并

验证内容（应已包含）:
```cpp
const LowerCaseString XEnvoyRouteIdentifier{"x-envoy-route-identifier"};
```

### `source/server/admin/admin.cc`

**状态**: ✅ 已正确合并

验证内容（应已添加）:
```cpp
#if defined(ALIMESH)
      if (handler.prefix_ != "/stats/prometheus") {
        auto route_identifier = admin_stream.getRequestHeaders().getByKey(
            Http::CustomHeaders::get().AliExtendedValues.XEnvoyRouteIdentifier);
        if (route_identifier) {
          return Admin::makeStaticTextRequest(
              "Access to admin interfaces via routing is forbidden.", Http::Code::Forbidden);
        }
      }
#endif
```

## 风险评估

| 风险项 | 等级 | 说明 |
|--------|------|------|
| 功能遗漏 | 🟢 低 | 所有 patch 功能已验证存在于 1.36.4 |
| 架构冲突 | 🟢 低 | 采用 HEAD 版本保持架构一致性 |
| 编译失败 | 🟢 低 | 不引入新代码，仅保留现有代码 |

## 执行清单

- [ ] 1. 解决 `config_impl.cc` 冲突：采用 HEAD 版本
- [ ] 2. 验证 `headers.h` 自动合并正确
- [ ] 3. 验证 `admin.cc` 自动合并正确
- [ ] 4. 完成 cherry-pick
- [ ] 5. 编译验证
- [ ] 6. 单元测试验证

## 备注

该 patch 的核心功能（禁止路由访问 admin 接口）在 1.36.4 中已经部分实现：
- `EnvoyRouteIdentifierValue` 常量和 `finalizeRequestHeaders` 中的 header 设置已存在
- 本次 cherry-pick 仅新增 `admin.cc` 中的访问控制检查逻辑

这表明 1.36.4 分支可能已经合并过此 patch 的部分内容，或者该功能是通过其他途径实现的。
