# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **日期** | 2026-01-26 |
| **原始提交** | `1753d9562c` |
| **Patch 名称** | 005-fix-fallback-cluster-bug.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **操作人** | AI Assistant |
| **最终结果** | ❌ 放弃 cherry-pick（功能已在新架构中实现） |

## 冲突概览

| 项目 | 状态 |
|------|------|
| 冲突文件数 | 2 |
| 编译问题 | 否（未执行） |
| 单测问题 | 否（未执行） |

## 放弃原因

**此 Patch 在 1.36.4 中不需要应用**

### 架构重构

1.27.7 到 1.36.4 之间发生了重大代码重构：

| 组件 | 1.27.7 位置 | 1.36.4 位置 |
|------|-------------|-------------|
| `DynamicRouteEntry` | `config_impl.h` | `delegating_route_impl.h` |
| `WeightedClusterEntry` | `config_impl.h` | `weighted_cluster_specifier.cc` |
| `pickWeightedCluster` | `config_impl.cc` | `weighted_cluster_specifier.cc` |

### Bug 不存在于 1.36.4

- 005 patch 修复的是 `clone()` 方法中 `owner_` 引用错误的 bug
- 1.36.4 的 `DynamicRouteEntry` **没有** `clone()` 方法和 `owner_` 成员
- 1.36.4 使用委托模式（`DelegatingRouteEntry`），架构完全不同

### Fallback 功能验证

- 1.36.4 的 `cluster_fallback` 插件使用新的 `DynamicRouteEntry` 委托模式
- 编译验证：✅ 通过
- 单元测试：✅ 2/2 测试通过

## 关键决策

| 决策 | 原因 |
|------|------|
| 放弃 cherry-pick | 相关代码已重构，bug 场景不存在 |
| 不添加 `clone()` 方法 | 1.36.4 的 fallback 机制不依赖 `clone()` |

## 验证记录

```bash
# 编译 cluster_fallback 插件
bazel build --config=clang //contrib/custom_cluster_plugins/cluster_fallback/...
# 结果: Build completed successfully

# 运行测试
bazel test --config=clang //contrib/custom_cluster_plugins/cluster_fallback/...
# 结果: Executed 2 out of 2 tests: 2 tests pass.
```

## 注意事项

1. 如果后续在 1.36.4 上发现 fallback 相关的 bug，需要在新架构中单独修复
2. 1.36.4 的 fallback 逻辑位于：
   - `weighted_cluster_specifier.cc` - 权重集群选择
   - `contrib/custom_cluster_plugins/cluster_fallback/source/filter.cc` - fallback 插件

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 详细冲突分析
- [x] commit_info.txt - 原始提交信息
- [x] modified_files.txt - 原 patch 修改的文件列表

---

**归档时间**: 2026-01-26
