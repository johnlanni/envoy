# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **归档日期** | 2026-01-26 |
| **原始提交** | `fd1950ce5b` |
| **源分支** | `1.27.7` |
| **目标分支** | `1.36.4` |
| **操作人** | jingze |

## 提交信息

```
commit fd1950ce5b03178e0c9a40f1be6563eb7cd683e2
Author: jingze <daijingze.djz@alibaba-inc.com>
Date:   Mon Jan 19 17:49:06 2026 +0800

    Apply patch: 013-fix-srds-direct-local-address.patch
    
    Change-Id: I26c6ec1ab5a36a348809145dbbc7c81962477b26
    Co-developed-by: Cursor <noreply@cursor.com>

 .../observability/access_log/usage.rst             | 40 ++++++++++++++++++++
 envoy/network/socket.h                             |  6 +++
 source/common/http/conn_manager_config.h           | 10 +++++
 source/common/http/conn_manager_impl.cc            | 16 +++++++-
 source/common/http/filter_manager.h                |  3 ++
 source/common/network/socket_impl.h                |  8 +++-
 source/common/router/scoped_config_impl.cc         |  2 +-
 source/extensions/filters/http/lua/wrappers.cc     |  7 ++++
 source/extensions/filters/http/lua/wrappers.h      | 10 ++++-
 .../formatter/substitution_formatter_test.cc       | 44 +++++++++++++++++++++-
 test/common/http/conn_manager_impl_test_base.cc    |  9 +++++
```

## 修改文件概览

共修改 5 个文件：

```
source/common/http/conn_manager_config.h
source/common/http/conn_manager_impl.cc
source/common/router/scoped_config_impl.cc
test/common/formatter/substitution_formatter_test.cc
test/common/http/conn_manager_impl_test_base.cc
```

## 冲突解决概览

- **冲突文件数**: 5 个
- **是否有编译问题**: 是（已解决）
- **是否有单测问题**: 否

### 冲突文件列表

| 文件 | 解决策略 |
|------|---------|
| `conn_manager_impl.cc` | 手动合并：保留 1.36.4 的 `route_result` 变量名 + 添加 patch 统计逻辑 |
| `wrappers.h` | 采用 HEAD：保留 1.36.4 的 `dynamicTypedMetadata`/`filterState` |
| `substitution_formatter_test.cc` | 采用 Incoming：删除重复测试代码 |
| `header_formatter_test.cc` | 采用 HEAD：保持 1.36.4 的 `UPSTREAM_METADATA(ns:key)` 语法 |
| `lua_integration_test.cc` | 采用 HEAD：保持 `std::get<0>(GetParam()).version` 参数化方式 |

## 关键决策

1. **条件编译宏统一**: 将 `#if defined(ALIMESH)` 改为 `#if defined(HIGRESS)`，保持与 1.36.4 代码库中其他 HIGRESS 相关代码的一致性

2. **变量名适配**: patch 中使用 `route` 变量，但 1.36.4 使用 `route_result` 结构体，需要适配为 `route_result.route`

3. **统计指标**: 新增的 `downstream_rq_retry_scope_found_total_` 和 `downstream_rq_retry_scope_not_found_total_` 统计指标需要在 `#if defined(HIGRESS)` 条件编译块中

4. **测试参数化差异**: 1.36.4 使用 `std::get<0>(GetParam()).version`，1.27.7 使用直接的 `GetParam()`，保持目标分支的测试框架

## 注意事项

- 统计指标 `downstream_rq_retry_scope_found/not_found_total_` 只在 `HIGRESS` 宏启用时可用
- `conn_manager_config.h` 中的统计定义也需要使用 `HIGRESS` 宏保护（已由用户手动修改）
- 该 patch 的核心功能是 SRDS 重试查找时记录统计信息，帮助监控路由重试行为

## 归档文件清单

- [x] SUMMARY.md (本文档)
- [x] commit_info.txt
- [x] modified_files.txt
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md



---

*归档时间: 2026-01-26 17:57:30*
