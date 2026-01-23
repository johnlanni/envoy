# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-01-23
- **原始提交**: 288664f2a9 (Apply patch: 000-5-internal-redirect-policy.patch)
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **最终提交**: 150b8cf040

## 冲突概览
- 冲突文件数: 9
- 编译问题: 是 (9个问题已修复)
- 单测问题: 是 (1个问题已修复)

## 关键决策

### 1. async_client_impl.h 架构重构处理
- **决策**: 采用 HEAD（不合并 Incoming 的类定义）
- **原因**: 1.36.4 中 `NullHedgePolicy`, `RouteEntryImpl`, `RouteImpl` 等结构已移至 `null_route_impl.h`
- **后续操作**: 在 `null_route_impl.h` 中添加 HIGRESS 的 `internalActiveRedirectPolicy()` 方法

### 2. config_impl.h 中 DynamicRouteEntry/WeightedClusterEntry 处理
- **决策**: 采用 HEAD（不合并这些类的 HIGRESS 代码）
- **原因**: 1.36.4 中 `DynamicRouteEntry` 已移至 `delegating_route_impl.h`，通过委托模式自动处理

### 3. API 适配
- **决策**: 修改调用方式适配 1.36.4 API
- **变更**:
  - `HeaderParser::configure()` → 使用 `THROW_OR_RETURN_VALUE`
  - `Regex::Utility::parseRegex()` → 添加 `regex_engine` 参数
  - `translateOpaqueConfig()` → 使用 `THROW_IF_NOT_OK`
  - `routeName()` → 从 `Route` 接口调用

### 4. Mock 文件修复
- **决策**: 在 `MockRoute` 类添加缺失的 `internalActiveRedirectPolicy()` 方法
- **原因**: 接口在 `RouteEntry` 中声明为 PURE virtual，需要在 Mock 中实现

## 注意事项
- `external_deps = ["abseil_optional"]` 在 1.36.4 中不存在，已移除
- `DynamicRouteEntry` 的 `clone()` 方法与 1.36.4 架构不兼容，已移除
- 需要添加 `path_utility.h` 头文件以使用 `PathUtil` 命名空间

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改文件列表

## 修改文件统计

### 冲突解决
| 文件 | 冲突数 | 解决策略 |
|------|--------|----------|
| envoy/router/BUILD | 1 | 采用 Incoming |
| envoy/router/router.h | 1 | 采用 Incoming |
| source/common/http/async_client_impl.h | 1 | 采用 HEAD |
| source/common/router/BUILD | 1 | 采用 Incoming |
| source/common/router/config_impl.cc | 1 | 手动合并 |
| source/common/router/config_impl.h | 2 | 采用 HEAD + 手动合并 |
| test/common/router/config_impl_test.cc | 3 | 手动合并 |
| test/common/router/router_test.cc | 2 | 手动合并 |
| test/common/router/router_test_base.h | 1 | 手动合并 |

### 编译修复
| 文件 | 修复内容 |
|------|---------|
| contrib/common/active_redirect/source/BUILD | 移除 external_deps |
| contrib/common/active_redirect/source/active_redirect_policy_impl.cc | API 适配 |
| contrib/common/active_redirect/source/active_redirect_policy_impl.h | 添加 regex_engine 参数 |
| source/common/http/async_client_impl.cc | 移除冗余静态成员 |
| source/common/router/config_impl.cc | 添加 regex_engine 参数 |
| source/common/router/config_impl.h | 移除不兼容的 clone() 方法 |
| source/common/router/router.cc | routeName() API 适配 + 添加头文件 |
| test/mocks/router/mocks.cc | 添加 ON_CALL 设置 |
| test/mocks/router/mocks.h | 添加 Mock 方法和成员变量 |

## 验证结果
- [x] 编译通过
- [x] 单元测试通过 (config_impl_test, router_test)
- [x] 条件编译正确 (HIGRESS)
