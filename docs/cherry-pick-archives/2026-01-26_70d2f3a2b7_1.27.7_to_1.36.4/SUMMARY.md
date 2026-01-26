# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **日期** | 2026-01-26 |
| **原始提交** | `70d2f3a2b7308fa3b4fa6c62310c36e095a392a4` |
| **Patch 名称** | 006-ensure-base-wasm-destructed-in-main-thread.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |

## 执行结果

**结果**: ⏭️ **已跳过** (`git cherry-pick --skip`)

**原因**: Patch 想要修复的问题（确保 base_wasm 在主线程析构）在 1.36.4 版本中已通过架构重构解决。

## 冲突概览

- **冲突文件数**: 3
- **编译问题**: 无（跳过）
- **单测问题**: 无（跳过）

### 冲突文件

| 文件 | 解决方案 |
|------|---------|
| `bazel/repository_locations.bzl` | 采用 HEAD（保持最新 proxy-wasm-cpp-host 版本）|
| `source/extensions/filters/http/wasm/wasm_filter.cc` | 采用 HEAD（功能已在父类实现）|
| `source/extensions/filters/http/wasm/wasm_filter.h` | 采用 HEAD（功能已在父类实现）|

## 关键决策

### 1. 确认 Patch 功能已在 1.36.4 实现

**决策**: Patch 不需要合并

**原因**:
- 1.27.7 的 `FilterConfig` 是独立类，patch 在其中添加 `base_wasm_handle_` 成员
- 1.36.4 的 `FilterConfig` 继承自 `PluginConfig`
- `PluginConfig` 已有 `base_wasm_` 成员并在 callback 中正确赋值
- 功能完全等效，无需额外修改

### 2. proxy-wasm-cpp-host 版本

**决策**: 保持 HEAD 版本 (`a0f625e4b84949bfb9c19c5742a3331e32c7cc1b`)

**原因**: 用户确认 1.36.4 使用的是最新版本

## 架构差异说明

### 1.27.7 架构
```
FilterConfig (独立类)
├── tls_slot_
├── remote_data_provider_
├── base_wasm_handle_  <-- Patch 添加
└── createFilter()
```

### 1.36.4 架构
```
PluginConfig (父类)
├── stats_handler_
├── plugin_
├── remote_data_provider_
├── base_wasm_  <-- 已有
├── plugin_handle_
└── createContext()

FilterConfig : public PluginConfig
└── (继承所有父类功能)
```

## 验证清单

- [x] 确认 `PluginConfig::base_wasm_` 在 callback 中被正确赋值 (`wasm.cc:687`)
- [x] 确认 `FilterConfig` 继承自 `PluginConfig` (`wasm_filter.h:19`)
- [x] 确认 `PluginConfig` 有 `base_wasm_` 成员 (`wasm.h:249`)
- [ ] 编译验证 - 跳过（无代码变更）
- [ ] 单测验证 - 跳过（无代码变更）

## 注意事项

1. 后续如果有基于 1.27.7 的其他 patch 依赖于 `FilterConfig::base_wasm_handle_`，需要注意在 1.36.4 中应使用 `PluginConfig::base_wasm_`（通过继承访问）
2. 1.36.4 的 wasm 相关代码经过重大重构，后续 cherry-pick 时需仔细分析架构差异

## 相关文件

- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 详细冲突分析
- [x] commit_info.txt - 原始提交信息
- [ ] COMPILATION_FIX_PLAN.md - 不适用
- [ ] TEST_FIX_PLAN.md - 不适用
