# Cherry-pick 归档总结

## 基本信息

- **日期**: 2026-01-23
- **原始提交**: `36a45f5258` - Apply patch: 000-6-redis-async-client.patch
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **操作人**: AI Assistant (Claude)

## 冲突概览

- **冲突文件数**: 2
- **编译问题**: 是（6 个问题）
- **单测问题**: 否

## 冲突文件列表

| 文件 | 冲突类型 | 解决方案 |
|------|---------|---------|
| `test/mocks/upstream/thread_local_cluster.cc` | 新增代码冲突 | 合并双方代码 |
| `test/extensions/filters/network/common/redis/codec_impl_test.cc` | 新增代码冲突 | 合并双方代码 |

## 编译修复

| 序号 | 问题类型 | 位置 | 修复方案 |
|------|---------|------|---------|
| 1 | 目标不存在 | `source/extensions/filters/network/common/redis/BUILD:118` | 移除 `//:examples_library` 引用 |
| 2 | 目标不存在 | `source/extensions/filters/network/common/redis/BUILD:102` | 移除 `//:examples_library` 引用 |
| 3 | 目标不存在 | `source/extensions/common/redis/BUILD:23` | 移除 `//:examples_library` 引用 |
| 4 | 目标路径变更 | `source/common/redis/BUILD:22` | 更改 `load_balancer_lib` 路径到 `source/extensions/load_balancing_policies/common` |
| 5 | 接口变更 | `source/common/redis/async_client_impl.cc:148` | 使用 `HostImpl::create()` 工厂方法替代构造函数 |
| 6 | 接口变更 | `source/common/redis/async_client_impl.cc:75` | 从 `HostSelectionResponse.host` 获取 host |

## 关键决策

1. **移除 examples_library 引用**: 1.36.4 分支没有定义 `//:examples_library` 目标，而 1.27.7 有。移除这些 visibility 引用不影响功能。

2. **load_balancer_lib 路径变更**: 在 1.36.4 中，`load_balancer_impl.cc/h` 从 `source/common/upstream` 移到了 `source/extensions/load_balancing_policies/common`。

3. **HostImpl 构造接口变化**: 1.36.4 使用工厂方法 `HostImpl::create()` 返回 `absl::StatusOr<std::unique_ptr<HostImpl>>`，而 1.27.7 直接使用构造函数。

4. **LoadBalancer::chooseHost() 返回值变化**: 1.36.4 返回 `HostSelectionResponse` 结构体，需要从 `.host` 成员获取实际的 host。

## 测试验证

- **编译测试**: ✅ 通过
- **单元测试**: ✅ 通过
  - `//test/extensions/filters/network/common/redis:codec_impl_test`
  - `//test/extensions/filters/network/common/redis:client_impl_test`

## 注意事项

1. 1.36.4 与 1.27.7 之间存在较大的架构差异，特别是在 upstream 模块
2. 后续 cherry-pick 可能会遇到类似的接口变更问题
3. 建议在 cherry-pick 前先检查相关模块的接口变化

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案
- [x] COMPILATION_FIX_PLAN.md - 编译修复计划
- [x] commit_info.txt - 提交信息
- [x] modified_files.txt - 修改的文件列表
