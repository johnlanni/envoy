# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **归档日期** | 2026-01-22 |
| **原始提交** | `9f11788ab6f2e14fac41db64928ac96dd6c53806` |
| **源分支** | `1.27.7-merge` |
| **目标分支** | `1.36.4-merge` |
| **补丁名称** | 000-3-srds.patch (SRDS 功能增强) |
| **操作人** | zhangty |

## 提交信息

```
commit 9f11788ab6f2e14fac41db64928ac96dd6c53806
Author: jingze <daijingze.djz@alibaba-inc.com>
Date:   Mon Jan 19 15:40:41 2026 +0800

    Apply patch: 000-3-srds.patch
    
    Change-Id: Ic38c6b82aa86524befd8b51e49086afa226723bf

24 files changed, 1471 insertions(+), 10 deletions(-)
```

## 修改文件概览

共修改 **25** 个文件（原始补丁24个 + 修复适配1个）：

### 源代码 (14个)
- `api/envoy/config/route/v3/route_components.proto`
- `api/envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.proto`
- `envoy/router/scopes.h`
- `source/common/http/conn_manager_config.h`
- `source/common/http/conn_manager_impl.cc`
- `source/common/http/conn_manager_impl.h`
- `source/common/http/conn_manager_utility.cc`
- `source/common/router/config_impl.cc`
- `source/common/router/config_impl.h`
- `source/common/router/scoped_config_impl.cc`
- `source/common/router/scoped_config_impl.h`
- `source/extensions/filters/network/http_connection_manager/config.cc`
- `source/extensions/filters/network/http_connection_manager/config.h`
- `source/server/admin/admin.h`

### 测试代码 (11个)
- `test/common/http/conn_manager_impl_fuzz_test.cc`
- `test/common/http/conn_manager_impl_test.cc`
- `test/common/http/conn_manager_impl_test_3.cc` (修复适配)
- `test/common/http/conn_manager_impl_test_base.cc`
- `test/common/http/conn_manager_impl_test_base.h`
- `test/common/router/config_impl_test.cc`
- `test/common/router/scoped_config_impl_test.cc`
- `test/common/router/scoped_rds_test.cc`
- `test/mocks/http/mocks.h`
- `test/mocks/router/mocks.cc`
- `test/mocks/router/mocks.h`

## 冲突解决概览

- **冲突文件数**: 10
- **是否有编译问题**: ✅ 是
- **是否有单测问题**: ✅ 是

### 冲突类型分布

| 类型 | 数量 | 说明 |
|------|------|------|
| 架构差异适配 | 3 | conn_manager_impl.cc 中 RouteConfigUpdateRequester 重构 |
| 代码合并 | 5 | 需要合并 HIGRESS 条件编译和 1.36 新特性 |
| 简单替换 | 2 | 直接采用 SRDS patch 版本 |

## 关键决策

### 决策 1: RouteConfigUpdateRequester 函数保留策略

**背景**: 在 `conn_manager_impl.cc` 中，SRDS patch 引入了 `RdsRouteConfigUpdateRequester` 相关函数，但 1.36 版本已将这些函数重构到独立文件 `route_config_update_requester.cc`。

**决策**: 删除 incoming 代码块中的 `RdsRouteConfigUpdateRequester` 函数，保留 HEAD（空）。

**原因**: 1.36 架构已经重构，这些函数已存在于其他文件中，直接合并会导致重复定义。

### 决策 2: HIGRESS 条件编译与 1.36 特性合并

**背景**: 多处冲突涉及 HIGRESS 条件编译代码与 1.36 新增特性的合并。

**决策**: 保留两者，通过条件编译方式共存。

**原因**: HIGRESS 特性需要保留，同时 1.36 的错误处理和新接口也需要保留。

### 决策 3: Proto 字段重复问题

**背景**: 编译时发现 `route_components.proto` 中 `cluster_specifier_plugin` 和 `inline_cluster_specifier_plugin` 字段重复定义。

**决策**: 删除 `WeightedCluster` 消息中的重复字段（保留 `Route` 中的定义）。

**原因**: Proto 字段号冲突会导致编译失败。

### 决策 4: 静态成员初始化问题

**背景**: HIGRESS 添加的 `SslRedirectRoute`、`SslPermanentRedirectRoute`、`SNIRedirectRoute` 类在 1.36 中无法正确初始化静态成员。

**决策**: 将静态成员改为实例成员，并适配 1.36 的 `Route` 接口。

**原因**: 1.36 的 `Route` 接口有变化，需要实现更多纯虚方法。

### 决策 5: 测试 Mock 签名适配

**背景**: SRDS 相关测试在 HIGRESS 模式下使用三参数版本的 `getRouteConfig`，但测试只 mock 了单参数版本。

**决策**: 为所有 SRDS 测试添加条件编译分支，根据 HIGRESS 模式选择正确的 mock 签名。

**原因**: 确保测试在 HIGRESS 和非 HIGRESS 模式下都能正确运行。

## 架构差异说明

| 组件 | 1.27 版本 | 1.36 版本 | 适配方式 |
|------|-----------|-----------|---------|
| `RouteConfigUpdateRequester` | 定义在 `conn_manager_impl.cc` | 重构到独立文件 `route_config_update_requester.cc` | 删除重复代码，使用 1.36 架构 |
| `ConnectionManagerConfig` | 引用类型 (`.`) | 智能指针类型 (`->`) | 更新成员访问方式 |
| `FilterManager::originalBufferedRequestData()` | 存在 | 已移除，改用 `bufferedRequestData()` | 替换方法调用 |
| `Route` 接口 | 较少纯虚方法 | 增加多个纯虚方法 | 实现所有纯虚方法 |

## 注意事项

1. **HIGRESS 条件编译**: 本次适配大量使用了 `#if defined(HIGRESS)` 条件编译，未来如果需要在非 HIGRESS 模式编译，需要确保相关代码路径正确。

2. **`originalBufferedRequestData` 替换**: 将 `originalBufferedRequestData()` 替换为 `bufferedRequestData()` 可能需要进一步验证功能正确性。

3. **SRDS 测试**: 4 个 SRDS 测试（`TestSrdsRouteNotFound`、`TestSrdsUpdate`、`TestSrdsCrossScopeReroute`、`TestSrdsRouteFound`）已添加条件编译分支。

4. **Proto 字段**: 删除了 `WeightedCluster` 中的重复字段，如果有依赖这些字段的代码需要更新。

## 后续工作

- [x] 冲突解决
- [x] 编译验证（非测试代码）
- [x] 单元测试验证（部分）
- [x] **`scoped_rds_test.cc` 编译修复** ✅ (2026-01-22 增量修复)
  - `OptionalHttpFilters` 类型未定义 → 移除参数，适配 1.36 架构
  - `nodiscard` 属性警告（17处） → 使用 `EXPECT_TRUE(fn().ok())`
  - `ProtobufWkt` 命名空间 → 改为 `Protobuf`
  - `createStaticConfigProvider` 参数类型 → 包装为 `ConstMessagePtrVector`
- [ ] 集成测试验证（如需要）
- [ ] `route_config_update_requster.cc` 中 HIGRESS 适配（**可选**，仅当使用依赖 StreamInfo 的复杂 scope key 计算时需要）

## 归档文件清单

- [x] SUMMARY.md (本文档)
- [x] commit_info.txt (提交信息)
- [x] modified_files.txt (修改文件列表)
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md (冲突解决方案)
- [x] COMPILATION_FIX_PLAN.md (编译修复计划)
- [x] TEST_FIX_PLAN.md (单测修复计划)

---

## 增量更新记录

| 日期 | 更新内容 | 原因 |
|------|---------|------|
| 2026-01-22 | 新增 `scoped_rds_test.cc` 修复 | 初次归档时遗漏该测试文件的编译问题 |

---

*初次归档时间: 2026-01-22 20:36:00*
*最后更新时间: 2026-01-22*
*生成工具: cherry-pick-conflict-resolution skill*
