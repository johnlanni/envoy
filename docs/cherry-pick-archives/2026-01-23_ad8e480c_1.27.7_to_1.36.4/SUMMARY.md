# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-01-23
- **原始提交**: ad8e480c84 (Apply patch: 000-4-custom-response-fallback.patch)
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **最终提交**: 86bbdfba78

## 冲突概览
- 冲突文件数: 6
- 编译问题: 是 (3个问题已修复)
- 单测问题: 是 (1个问题已修复)

## 关键决策

### 1. 接口类型差异处理
- **决策**: 保留 1.36 的 `uint64_t` 类型和 `Upstream::LoadBalancerContext::OverrideHost` 类型
- **原因**: 1.36 的类型更通用，向后兼容

### 2. API 变化适配
- **决策**: 将 `getByKey` API 调用改为 `get` API
- **原因**: 1.36 版本中 `HeaderMap` 接口变化

### 3. 配置遍历 API
- **决策**: 保留 1.36 的 `getAllPerFilterConfig<T>()` API，添加 HIGRESS 条件检查
- **原因**: 1.36 的新 API 更高效

### 4. 缓冲区逻辑整合
- **决策**: 保留 1.36 的改进缓冲区逻辑，在 HIGRESS 条件中添加 `needBuffering()` 支持
- **原因**: 1.36 的逻辑更完善，同时保留 HIGRESS 功能

## 注意事项
- Proto 文件中存在预先存在的重复字段定义问题，已一并修复
- 所有 HIGRESS 条件编译代码已正确适配 1.36 接口

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] TEST_FIX_PLAN.md - 单测修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改文件列表

## 修改文件统计

### 冲突解决
| 文件 | 冲突数 |
|------|--------|
| envoy/http/filter.h | 1 |
| source/common/http/filter_manager.h | 3 |
| source/common/http/filter_manager.cc | 1 |
| source/common/router/router.cc | 1 |
| source/extensions/filters/http/custom_response/custom_response_filter.cc | 1 |
| test/mocks/http/mocks.h | 1 |

### 编译修复
| 文件 | 修复内容 |
|------|---------|
| api/envoy/config/route/v3/route_components.proto | 删除重复字段定义 |
| source/extensions/http/custom_response/redirect_policy/redirect_policy.cc | getByKey → get |
| source/extensions/filters/http/custom_response/custom_response_filter.cc | 添加 grpc/common.h |
| source/extensions/filters/http/custom_response/BUILD | 添加 grpc:common_lib 依赖 |

### 测试修复
| 文件 | 修复内容 |
|------|---------|
| test/extensions/filters/http/custom_response/custom_response_filter_test.cc | getByKey → get |

## 验证结果
- [x] 编译通过
- [x] 单元测试通过 (filter_manager_test, custom_response_filter_test)
- [x] 条件编译正确 (HIGRESS)
