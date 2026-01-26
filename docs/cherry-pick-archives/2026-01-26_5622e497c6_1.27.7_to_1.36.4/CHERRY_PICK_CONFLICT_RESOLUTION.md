# Cherry-pick 冲突解决方案

## 概述

| 项目 | 内容 |
|------|------|
| **原始提交** | `5622e497c6` |
| **提交信息** | Apply patch: 007-optimize-custom-response-filter-for-ai-fallback.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **冲突文件数** | 1 |
| **自动合并文件数** | 7 |

## Patch 功能说明

该 patch 的主要目的是优化 custom_response_filter 以支持 AI fallback 场景：

1. **移除 `has_rules_` 优化检查**：原先有一个 `has_rules_` 标志用于在 HIGRESS 构建时跳过无规则情况下的配置遍历，patch 移除了这个优化（可能是因为在 AI fallback 场景下有特殊需求）
2. **简化 decodeHeaders() 逻辑**：移除了 `has_rules_` 的设置逻辑
3. **其他改动**：redirect_policy、conn_manager 等相关优化（已自动合并）

## 自动合并文件列表

以下文件已成功自动合并，无需手动处理：

| 文件 | 改动类型 |
|------|---------|
| `api/envoy/extensions/http/custom_response/redirect_policy/v3/redirect_policy.proto` | 新增字段 |
| `source/common/http/conn_manager_impl.cc` | 逻辑增强 |
| `source/common/http/conn_manager_utility.cc` | 逻辑增强 |
| `source/common/http/headers.h` | 新增 header |
| `source/extensions/filters/http/custom_response/custom_response_filter.h` | 移除 `has_rules_` 成员 |
| `source/extensions/http/custom_response/redirect_policy/redirect_policy.cc` | 功能优化 |
| `source/extensions/http/custom_response/redirect_policy/redirect_policy.h` | 接口更新 |

## 冲突文件详情

### 文件 1: `source/extensions/filters/http/custom_response/custom_response_filter.cc`

**冲突位置**: 第 77-107 行（1 个冲突块）

**冲突类型**: API 架构差异

#### 冲突块分析

**HEAD (1.36.4 目标分支)**:
```cpp
#if defined(HIGRESS)
  if (has_rules_) {
#endif
    for (const FilterConfig& typed_config :
         Http::Utility::getAllPerFilterConfig<FilterConfig>(encoder_callbacks_)) {
      // Check if a match is found first to avoid overwriting policy with an
      // empty shared_ptr.
      auto maybe_policy = typed_config.getPolicy(headers, encoder_callbacks_->streamInfo());
      if (maybe_policy) {
        policy = maybe_policy;
      }
    }
#if defined(HIGRESS)
  }
#endif
```

**Incoming (1.27.7 patch)**:
```cpp
  decoder_callbacks_->traversePerFilterConfig(
      [&policy, &headers, this](const Router::RouteSpecificFilterConfig& config) {
        const FilterConfig* typed_config = dynamic_cast<const FilterConfig*>(&config);
        if (typed_config) {
          // Check if a match is found first to avoid overwriting policy with an
          // empty shared_ptr.
          auto maybe_policy = typed_config->getPolicy(headers, encoder_callbacks_->streamInfo());
          if (maybe_policy) {
            policy = maybe_policy;
          }
        }
      });
```

#### 差异分析

| 维度 | 1.36.4 (HEAD) | 1.27.7 (Patch) |
|------|--------------|----------------|
| **遍历 API** | `Http::Utility::getAllPerFilterConfig<FilterConfig>()` (新 API) | `decoder_callbacks_->traversePerFilterConfig()` (旧 API) |
| **迭代方式** | range-based for 循环 | lambda 回调 |
| **类型转换** | 直接获取 `FilterConfig&` | 使用 `dynamic_cast` 转换 |
| **`has_rules_` 保护** | 有 (`#if defined(HIGRESS)`) | **已移除** |

#### Patch 意图

Patch 的核心意图是：**移除 `has_rules_` 条件保护**，使配置遍历始终执行。

这个改动需要配合头文件中删除 `has_rules_` 成员变量（已自动合并）。

#### 解决方案

✅ **采用 HEAD (1.36.4) 的 API，应用 Patch 的意图移除 `has_rules_` 保护**

理由：
1. 1.36.4 使用的是更新的 API (`getAllPerFilterConfig`)，应保留
2. Patch 的核心改动是移除 `has_rules_` 保护，这个意图需要应用
3. 头文件中 `has_rules_` 已被删除（自动合并），如果保留 `.cc` 文件中的引用会导致编译错误

**合并后代码**:
```cpp
  PolicySharedPtr policy;
  for (const FilterConfig& typed_config :
       Http::Utility::getAllPerFilterConfig<FilterConfig>(encoder_callbacks_)) {
    // Check if a match is found first to avoid overwriting policy with an
    // empty shared_ptr.
    auto maybe_policy = typed_config.getPolicy(headers, encoder_callbacks_->streamInfo());
    if (maybe_policy) {
      policy = maybe_policy;
    }
  }
```

## 风险评估

| 风险项 | 风险等级 | 说明 |
|--------|---------|------|
| API 兼容性 | 🟢 低 | 保留 1.36.4 的新 API，无兼容性问题 |
| 功能正确性 | 🟡 中 | 移除 `has_rules_` 可能影响性能，但这是 patch 的意图 |
| 编译问题 | 🟢 低 | 头文件已正确删除 `has_rules_`，`.cc` 文件对应修改后应无问题 |

## 验证清单

### 编译验证
- [ ] `bazel build --config=clang //source/extensions/filters/http/custom_response:config`

### 功能验证
- [ ] 确认配置遍历逻辑正确
- [ ] 确认 AI fallback 场景正常工作

## 执行步骤

```bash
# 1. 编辑冲突文件，应用解决方案
# 2. 标记冲突已解决
git add source/extensions/filters/http/custom_response/custom_response_filter.cc
# 3. 继续 cherry-pick
git cherry-pick --continue
```
