# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **日期** | 2026-01-26 |
| **原始提交** | `58535e248c0df88fb3b4041359e8ca60c0ef7b27` |
| **提交信息** | Apply patch: 003-forbid-access-to-admin-interface.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |

## 冲突概览

| 统计项 | 值 |
|--------|-----|
| 冲突文件数 | 1 |
| 自动合并文件 | 2 |
| 编译问题 | 否 |
| 单测问题 | 否 |

## 修改文件

- `source/common/http/headers.h` - 新增 `XEnvoyRouteIdentifier` header 定义
- `source/server/admin/admin.cc` - 新增 admin 访问控制检查逻辑

## 关键决策

### 决策 1: 采用 HEAD 版本解决 `config_impl.cc` 冲突

**原因**: 
- Patch 需要添加的 `EnvoyRouteIdentifierValue` 常量和 `finalizeRequestHeaders` 中的代码**已存在于 1.36.4**
- Incoming 版本包含的 `mergeTransforms`、`createAndValidateRoute` 等函数是 1.27.7 的旧架构
- 这些函数在 1.36.4 中已重构到其他位置（`config_utility.cc`、`RouteCreator` 类）
- 采用 HEAD 版本避免引入重复代码和架构混乱

## Patch 功能说明

该 patch 的目的是**禁止通过路由访问 admin 接口**：

1. 定义 `XEnvoyRouteIdentifier` HTTP header
2. 在路由处理时设置该 header 标识请求经过了路由（已存在于 1.36.4）
3. 在 admin 接口检查该 header，如果存在则拒绝访问（`/stats/prometheus` 除外）

## 架构差异说明

1.27.7 和 1.36.4 之间存在重大架构重构：

| 代码 | 1.27.7 位置 | 1.36.4 位置 |
|------|-------------|-------------|
| `mergeTransforms()` | `config_impl.cc` 匿名命名空间 | `config_utility.cc` |
| `createAndValidateRoute()` | `config_impl.cc` 匿名命名空间 | `RouteCreator` 类静态方法 |
| `EnvoyRouteIdentifierValue` | 需要新增 | 已存在（第 131 行）|
| `finalizeRequestHeaders` 中的 HIGRESS 代码 | 需要新增 | 已存在（第 956-959 行）|

## 验证结果

- ✅ 编译验证通过
- ✅ 单元测试通过（`//test/server/admin:admin_test`）

## 注意事项

- 该 patch 的部分功能（`EnvoyRouteIdentifierValue`、`finalizeRequestHeaders` 中的代码）在 1.36.4 中已存在
- 本次 cherry-pick 实际仅新增了 `admin.cc` 中的访问控制检查逻辑和 `headers.h` 中的 header 定义

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案详细文档
- [x] commit_info.txt - 原始提交信息
- [x] modified_files.txt - 修改的文件列表
