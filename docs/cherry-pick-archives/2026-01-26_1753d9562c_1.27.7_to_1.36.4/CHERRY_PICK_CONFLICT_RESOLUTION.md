# Cherry-pick 冲突解决方案文档

## 1. 概述

| 项目 | 值 |
|------|-----|
| 原始提交 | `1753d9562c` |
| 提交信息 | Apply patch: 005-fix-fallback-cluster-bug.patch |
| 源分支 | 1.27.7 |
| 目标分支 | 1.36.4 |
| 冲突文件数 | 2 |
| 冲突类型 | 架构重构导致的代码位置变更 |

## 2. 冲突文件列表

| 文件 | 冲突行数 | 冲突类型 | 风险等级 |
|------|---------|---------|---------|
| `source/common/router/config_impl.cc` | 1272-1369 (~97行) | 代码移除/重构 | 🔴 高 |
| `source/common/router/config_impl.h` | 851-1128 (~277行) | 代码移除/重构 | 🔴 高 |

## 3. 架构差异分析

### 3.1 重大架构变更

1.27.7 到 1.36.4 之间发生了**重大代码重构**：

| 组件 | 1.27.7 位置 | 1.36.4 位置 |
|------|-------------|-------------|
| `DynamicRouteEntry` 类 | `config_impl.h` | `delegating_route_impl.h` |
| `WeightedClusterEntry` 类 | `config_impl.h` | `weighted_cluster_specifier.cc` |
| `pickWeightedCluster` 函数 | `config_impl.cc` | `weighted_cluster_specifier.cc` |
| `WeightedClustersConfig` 结构体 | `config_impl.h` | 已被 `WeightedClustersConfigEntry` 替代 |

### 3.2 `DynamicRouteEntry` 类变化

**1.27.7 版本** (在 `config_impl.h`):
```cpp
#if defined(HIGRESS)
class DynamicRouteEntry : public RouteEntryAndRoute,
                          public std::enable_shared_from_this<DynamicRouteEntry> {
#else
class DynamicRouteEntry : public RouteEntryAndRoute {
#endif
public:
    DynamicRouteEntry(const RouteEntryAndRoute* parent, RouteConstSharedPtr owner,
                      const std::string& name)
        : parent_(parent), owner_(std::move(owner)), cluster_name_(name) {}
    
    // ... 大量委托方法 ...
    
#if defined(HIGRESS)
    RouteConstSharedPtr clone(const std::string& name) const {
      return std::make_shared<DynamicRouteEntry>(parent_, owner_, name);
    }
    virtual RouteConstSharedPtr getRouteConstSharedPtr() const { return shared_from_this(); }
#endif

private:
    const RouteEntryAndRoute* parent_;
    const RouteConstSharedPtr owner_;  // 关键：持有 owner 的引用
    const std::string cluster_name_;
};
```

**1.36.4 版本** (在 `delegating_route_impl.h`):
```cpp
class DynamicRouteEntry : public DelegatingRouteEntry {
public:
  DynamicRouteEntry(RouteConstSharedPtr route, std::string&& cluster_name)
      : DelegatingRouteEntry(std::move(route)), cluster_name_(std::move(cluster_name)) {}

  const std::string& clusterName() const override { return cluster_name_; }

private:
  const std::string cluster_name_;
};
```

**关键差异**:
1. 1.36.4 版本**没有** `clone()` 方法
2. 1.36.4 版本**没有** `owner_` 成员
3. 1.36.4 使用委托模式（`DelegatingRouteEntry`），而非直接持有 parent 指针
4. 1.36.4 版本简洁很多，通过 `DelegatingRouteEntry` 实现委托功能

### 3.3 Fallback 机制变化

**1.27.7 版本** (在 `config_impl.cc` 的 `pickWeightedCluster`):
```cpp
#if defined(HIGRESS)
if (cluster_specifier_plugin_ != nullptr) {
    auto request_header = dynamic_cast<const Http::RequestHeaderMap*>(&headers);
    if (!cluster->clusterHeaderName().get().empty() &&
        !headers.get(cluster->clusterHeaderName()).empty()) {
      auto route = pickClusterViaClusterHeader(...);
      return cluster_specifier_plugin_->route(route, *request_header);
    }
    // 005 patch 修复的位置：
    auto route = std::make_shared<DynamicRouteEntry>(cluster.get(), shared_from_this(),
                                                     cluster->clusterName());
    return cluster_specifier_plugin_->route(route, *request_header);
}
#endif
```

**1.36.4 版本** (在 `weighted_cluster_specifier.cc` 的 `pickWeightedCluster`):
```cpp
RouteConstSharedPtr selected_route;
if (!cluster->cluster_name_.empty()) {
  selected_route = std::make_shared<WeightedClusterEntry>(std::move(parent), "", cluster);
} else {
  ASSERT(!cluster->cluster_header_name_.get().empty());
  const auto entries = headers.get(cluster->cluster_header_name_);
  absl::string_view cluster_name = entries.empty() ? absl::string_view{} : entries[0]->value().getStringView();
  selected_route = std::make_shared<WeightedClusterEntry>(std::move(parent), std::string(cluster_name), cluster);
}
#if defined(HIGRESS)
// Apply fallback plugin if configured
if (fallback_cluster_specifier_plugin_ != nullptr) {
  return fallback_cluster_specifier_plugin_->route(selected_route, headers);
}
#endif
return selected_route;
```

## 4. 原始 Patch 分析

### 4.1 Patch 目的

005-fix-fallback-cluster-bug.patch 修复的是 **`clone()` 方法中 owner 引用错误** 的 bug：

```cpp
// 原代码（有 bug）：
RouteConstSharedPtr clone(const std::string& name) const {
  return std::make_shared<DynamicRouteEntry>(parent_, shared_from_this(), name);
  //                                          ^^^^ 错误：使用 shared_from_this() 导致循环引用或生命周期问题
}

// 修复后：
RouteConstSharedPtr clone(const std::string& name) const {
  return std::make_shared<DynamicRouteEntry>(parent_, owner_, name);
  //                                          ^^^^ 正确：使用原有的 owner_
}
```

### 4.2 Patch 同时修复的问题

在 `pickWeightedCluster` 中，原代码直接传递 `cluster` 给 fallback plugin，修复后先创建 `DynamicRouteEntry`：

```cpp
// 原代码：
return cluster_specifier_plugin_->route(cluster, *request_header);

// 修复后：
auto route = std::make_shared<DynamicRouteEntry>(cluster.get(), shared_from_this(),
                                                 cluster->clusterName());
return cluster_specifier_plugin_->route(route, *request_header);
```

## 5. 解决方案

### ✅ 判定：此 Patch 在 1.36.4 中**不需要应用**

**理由**：

1. **架构完全重构**：
   - 相关代码已从 `config_impl.h/cc` 移到 `weighted_cluster_specifier.cc` 和 `delegating_route_impl.h`
   - 1.36.4 的 `DynamicRouteEntry` 完全不同，没有 `clone()` 方法和 `owner_` 成员

2. **Bug 不存在于 1.36.4**：
   - 1.36.4 的 fallback 机制（`cluster_fallback` 插件）直接创建新的 `DynamicRouteEntry`
   - 不调用任何 `clone()` 方法
   - 见 `contrib/custom_cluster_plugins/cluster_fallback/source/filter.cc`

3. **Fallback 逻辑已正确实现**：
   ```cpp
   // 1.36.4 的 cluster_fallback 实现 (filter.cc)
   return std::make_shared<Envoy::Router::DynamicRouteEntry>(std::move(route),
                                                             std::string(cluster_name));
   ```
   使用委托模式，route 的生命周期通过 `std::move(route)` 正确管理

4. **`pickWeightedCluster` 已正确处理**：
   ```cpp
   // weighted_cluster_specifier.cc
   selected_route = std::make_shared<WeightedClusterEntry>(std::move(parent), "", cluster);
   if (fallback_cluster_specifier_plugin_ != nullptr) {
     return fallback_cluster_specifier_plugin_->route(selected_route, headers);
   }
   ```
   已经正确创建 `WeightedClusterEntry`（继承自 `DynamicRouteEntry`）后传给 fallback plugin

## 6. 建议操作

1. **放弃此次 cherry-pick**：
   ```bash
   git cherry-pick --abort
   ```

2. **验证 1.36.4 的 fallback 功能正常工作**（如有测试用例）

## 7. 风险评估

| 风险项 | 评估 | 说明 |
|--------|------|------|
| 功能缺失 | 🟢 低 | 1.36.4 已有等效功能 |
| 潜在 bug | 🟢 低 | 1.36.4 架构避免了该 bug |
| 回归风险 | 🟢 低 | 不修改任何代码 |

## 8. 验证清单

- [ ] 确认 1.36.4 的 `cluster_fallback` 插件测试通过
- [ ] 确认 weighted cluster + fallback 场景正常工作
- [ ] 确认没有 `clone()` 相关的功能依赖

---

**文档生成时间**: 2026-01-26
**分析人**: AI Assistant
