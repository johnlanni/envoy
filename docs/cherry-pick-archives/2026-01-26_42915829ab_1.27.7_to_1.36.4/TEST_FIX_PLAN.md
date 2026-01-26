# 单测修复计划

## 概述

Cherry-pick 提交 `42915829ab` (008-optimize-srds-for-route-fallback.patch) 后，部分单测失败需要修复。

**最终状态**: ✅ 全部修复完成

## 已修复的问题

### 1. admin.cc 编译错误

**问题**: `getByKey` 方法在 1.36.4 中不存在
**文件**: `source/server/admin/admin.cc`
**修复**: 将 `getByKey()` 改为 `get()` 方法，并更新检查条件

### 2. ConnectionManagerConfigProxyObject 缺少方法

**问题**: 缺少 `retryOtherScopeWhenNotFound()` 方法
**文件**: `test/common/http/conn_manager_impl_test_base.cc`
**修复**: 添加方法代理实现

### 3. scoped_rds_test.cc 编译错误

**问题**: `VirtualHostRoute` 类型不能与 `nullptr` 直接比较
**文件**: `test/common/router/scoped_rds_test.cc`
**修复**: 改为比较 `.route` 成员

### 4. conn_manager_impl_test_3.cc EXPECT_CALL 参数和调用次数

**问题**: 
1. `getRouteConfig` mock 使用了 3 参数版本，但代码调用 4 参数版本
2. HIGRESS 优化跳过了 `decodeHeaders()` 中的 `snapScopedRouteConfig()` 调用，导致 mock 期望的调用次数不匹配
3. 测试未禁用 `retryOtherScopeWhenNotFound` 功能，导致额外的重试调用

**文件**: `test/common/http/conn_manager_impl_test_3.cc`

**修复**:
1. 更新所有 HIGRESS 条件编译块中的 EXPECT_CALL 为 4 参数版本 `getRouteConfig(_, _, _, _)`
2. 调整 mock 期望的调用次数，反映 HIGRESS 优化跳过 `decodeHeaders()` 中的 `snapScopedRouteConfig()` 调用:
   - `TestSrdsRouteNotFound`: Times(2) → Times(1)
   - `TestSrdsUpdate`: Times(3) → Times(2)
   - `TestSrdsCrossScopeReroute`: Times(3) → Times(2)
   - `TestSrdsRouteFound`: Times(2) → Times(1)
3. 在所有 SRDS 测试开头添加 `retry_other_scope_when_not_found_ = false;` 禁用重试功能
4. 更新 `TestSrdsCrossScopeReroute` 中 lambda 的签名以匹配 4 参数版本

**根因分析**:
- HIGRESS 条件编译下，`decodeHeaders()` 中设置 `snapped_route_config_ = std::make_shared<Router::NullConfigImpl>()` 而不是调用 `snapScopedRouteConfig()`
- 这导致 `getRouteConfig` 的调用次数比非 HIGRESS 版本少一次
- 新增的 `retryOtherScopeWhenNotFound` 功能会在路由未找到时重试，干扰了原有测试的 mock 预期

## 修改的文件列表

| 文件 | 修改类型 |
|------|----------|
| `source/server/admin/admin.cc` | 编译修复 |
| `test/common/http/conn_manager_impl_test_base.cc` | 添加方法 |
| `test/common/router/scoped_rds_test.cc` | 类型修复 |
| `test/common/http/conn_manager_impl_test_3.cc` | EXPECT_CALL 参数修复 + 调用次数修复 + 禁用重试 |

## 验证结果

所有 SRDS 相关测试通过:
- ✅ TestSrdsRouteNotFound
- ✅ TestSrdsUpdate
- ✅ TestSrdsCrossScopeReroute
- ✅ TestSrdsRouteFound
