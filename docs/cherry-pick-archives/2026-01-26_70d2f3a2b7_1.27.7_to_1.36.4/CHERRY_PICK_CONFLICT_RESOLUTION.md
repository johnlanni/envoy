# Cherry-pick 冲突解决方案

## 概述

| 项目 | 值 |
|------|-----|
| **原始提交** | `70d2f3a2b7` |
| **提交信息** | Apply patch: 006-ensure-base-wasm-destructed-in-main-thread.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **冲突文件数** | 3 |

## Patch 目的

确保 `base_wasm` 在主线程中被析构。通过在 `FilterConfig` 中保存 `base_wasm_handle_` 的引用，当 `FilterConfig` 在主线程销毁时，`base_wasm` 也会在主线程析构。

## 冲突文件列表

| 文件路径 | 冲突位置 | 冲突类型 |
|----------|---------|---------|
| `bazel/repository_locations.bzl` | 1523-1529 | 版本冲突 |
| `source/extensions/filters/http/wasm/wasm_filter.cc` | 14-38 | 架构差异 |
| `source/extensions/filters/http/wasm/wasm_filter.h` | 24-84 | 架构差异 |

---

## 冲突分析

### 冲突 1: bazel/repository_locations.bzl

**位置**: 1523-1529 行

**冲突类型**: proxy-wasm-cpp-host 版本冲突

#### HEAD (1.36.4):
```bzl
version = "a0f625e4b84949bfb9c19c5742a3331e32c7cc1b",
sha256 = "6e36909f9c888aba56d71b17b07a0a14ab38f89c9fef5036da2d6e7a9ddc27e9",
```

#### Incoming (1.27.7 patch):
```bzl
version = "ef59c0433755e8702b5537a092f3733d8189286f",
sha256 = "8d0b36873bff970449dafd84f0c122858fb415ba5dc60c82ec45e6fddb46dc58",
```

#### 解决方案: ✅ 采用 HEAD 版本

**原因**: 
- 用户明确指出 1.36.4 使用的是最新版本的 proxy-wasm-cpp-host
- 1.36.4 的版本 `a0f625e4...` 应该已经包含或超越了 patch 版本 `ef59c043...` 的功能
- 保持与当前 1.36.4 分支的一致性

---

### 冲突 2: source/extensions/filters/http/wasm/wasm_filter.cc

**位置**: 14-38 行

**冲突类型**: 架构重大差异

#### HEAD (1.36.4):
```cpp
FilterConfig::FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
                           Server::Configuration::UpstreamFactoryContext& context)
    : Extensions::Common::Wasm::PluginConfig(
          config.config(), context.serverFactoryContext(), context.scope(), context.initManager(),
          envoy::config::core::v3::TrafficDirection::OUTBOUND, nullptr, false) {}
```

1.36.4 中 `FilterConfig` 继承自 `PluginConfig`，构造函数只是调用父类构造函数。

#### Incoming (1.27.7 patch):
```cpp
auto callback = [plugin, this](const Common::Wasm::WasmHandleSharedPtr& base_wasm) {
    base_wasm_handle_ = base_wasm;  // <-- Patch 想要添加的行
    // NB: the Slot set() call doesn't complete inline, so all arguments must outlive this call.
    tls_slot_->set([base_wasm, plugin](Event::Dispatcher& dispatcher) {
      return std::make_shared<PluginHandleSharedPtrThreadLocal>(
          Common::Wasm::getOrCreateThreadLocalPlugin(base_wasm, plugin, dispatcher));
    });
};
...
```

1.27.7 中 `FilterConfig` 有自己的完整实现，包含 `tls_slot_`、`callback` 等。

#### 解决方案: ✅ 采用 HEAD 版本

**原因 - 架构分析**:

在 1.36.4 中，`FilterConfig` 的功能已被重构到父类 `PluginConfig` 中。查看 `source/extensions/common/wasm/wasm.cc` 第 686-687 行：

```cpp
auto callback = [this, &context](WasmHandleSharedPtr base_wasm) {
    base_wasm_ = base_wasm;  // <-- 已经实现了 patch 的功能！
    ...
};
```

**结论**: Patch 想要修复的问题（确保 base_wasm 在主线程析构）**在 1.36.4 中已经通过父类 `PluginConfig` 实现了**。`PluginConfig` 类有 `base_wasm_` 成员变量，并在 callback 中正确赋值。

---

### 冲突 3: source/extensions/filters/http/wasm/wasm_filter.h

**位置**: 24-84 行

**冲突类型**: 架构重大差异

#### HEAD (1.36.4):
```cpp
FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
               Server::Configuration::UpstreamFactoryContext& context);
```

只有一个新的构造函数声明（用于 UpstreamFactoryContext）。

#### Incoming (1.27.7 patch):
```cpp
std::shared_ptr<Context> createFilter() {
    // 大量实现代码...
}

private:
  ThreadLocal::TypedSlotPtr<PluginHandleSharedPtrThreadLocal> tls_slot_;
  Config::DataSource::RemoteAsyncDataProviderPtr remote_data_provider_;
  Envoy::Extensions::Common::Wasm::WasmHandleSharedPtr base_wasm_handle_;  // <-- Patch 想添加的成员
```

1.27.7 中 `FilterConfig` 有完整的实现，包括 `createFilter()` 方法和私有成员。

#### 解决方案: ✅ 采用 HEAD 版本

**原因 - 架构分析**:

查看 1.36.4 中 `PluginConfig` 类（`source/extensions/common/wasm/wasm.h` 第 227-251 行）：

```cpp
class PluginConfig {
private:
  // ... 其他成员 ...
  WasmHandleSharedPtr base_wasm_{};  // <-- 已经有这个成员！
  // ...
};
```

1.36.4 的 `PluginConfig` 父类已经包含：
1. `base_wasm_` 成员变量（等同于 patch 的 `base_wasm_handle_`）
2. `createContext()` 方法（等同于 patch 的 `createFilter()` 功能）
3. 完整的 wasm 生命周期管理

---

## 解决方案汇总

| 文件 | 解决方案 | 原因 |
|------|---------|------|
| `bazel/repository_locations.bzl` | ✅ 采用 HEAD | 保持最新版本 |
| `source/extensions/filters/http/wasm/wasm_filter.cc` | ✅ 采用 HEAD | 功能已在父类实现 |
| `source/extensions/filters/http/wasm/wasm_filter.h` | ✅ 采用 HEAD | 功能已在父类实现 |

## 关键发现

**本次 cherry-pick 实际上是一个"空操作"**：

1.27.7 的 patch 006 想要解决的问题是：
> 确保 base_wasm 在主线程中被析构

但在 1.36.4 中，由于架构重构：
- `FilterConfig` 继承自 `PluginConfig`
- `PluginConfig` 已经有 `base_wasm_` 成员
- `PluginConfig` 构造函数中已有 `base_wasm_ = base_wasm;` 赋值逻辑

**这意味着 patch 想要修复的 bug 在 1.36.4 中已经被修复了，无需额外修改。**

---

## 风险评估

| 风险项 | 级别 | 说明 |
|--------|------|------|
| 功能缺失 | 🟢 低 | 父类已实现相同功能 |
| 回归问题 | 🟢 低 | 保持 HEAD 不引入新代码 |
| 依赖版本 | 🟢 低 | 保持最新版本的 proxy-wasm-cpp-host |

## 验证清单

- [ ] 确认 `PluginConfig::base_wasm_` 在 callback 中被正确赋值
- [ ] 确认 `FilterConfig` 继承自 `PluginConfig`
- [ ] 确认编译通过
- [ ] 确认单元测试通过

---

## 执行步骤

1. 解决冲突 - 全部采用 HEAD 版本
2. 完成 cherry-pick
3. 编译验证
4. 单元测试验证

**注意**: 由于所有文件都采用 HEAD 版本，实际上这个 cherry-pick 不会引入任何代码变更。最终效果相当于跳过此 patch。

---

## 最终执行结果

**执行时间**: 2026-01-26

**结果**: ✅ Cherry-pick 已跳过 (`git cherry-pick --skip`)

**原因**: Patch 006 想要解决的问题（确保 base_wasm 在主线程析构）在 1.36.4 版本中已通过架构重构在父类 `PluginConfig` 中实现。无需任何代码变更。

### 验证清单完成状态

| 验证项 | 状态 | 证据位置 |
|--------|------|----------|
| `PluginConfig::base_wasm_` 在 callback 中被正确赋值 | ✅ | `source/extensions/common/wasm/wasm.cc:687` |
| `FilterConfig` 继承自 `PluginConfig` | ✅ | `source/extensions/filters/http/wasm/wasm_filter.h:19` |
| `PluginConfig` 有 `base_wasm_` 成员 | ✅ | `source/extensions/common/wasm/wasm.h:249` |
| 编译验证 | ⏭️ 跳过 | 无代码变更，无需编译 |
| 单元测试验证 | ⏭️ 跳过 | 无代码变更，无需测试 |
