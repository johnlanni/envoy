# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `0c0fc31187` - Apply patch: 017-misc-opt-of-wasm-rebuild-mem-and-pb-cache.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 9 个

## Patch 功能概述

该 patch 主要实现以下功能：
1. **Wasm VM 重建机制增强**: 区分 crash recovery 和主动 memory rebuild
2. **Protobuf Hash 缓存**: 优化 protobuf message 的 hash 计算性能
3. **PLUGIN_VM_MEMORY property**: 新增获取 VM 内存使用量的属性
4. **Request Headers Fallback**: 支持从 StreamInfo 获取请求头

---

## 🔴 架构差异警告

> **重要**: 1.27.7 和 1.36.4 之间存在显著的架构重构，直接 cherry-pick 需要大量适配工作。

### 架构对比

| 组件 | 1.27.7 (源分支) | 1.36.4 (目标分支) |
|------|----------------|------------------|
| FilterConfig 结构 | 独立类，包含 `createFilter()` 方法 | 继承 `PluginConfig` 基类 |
| PluginHandleSharedPtrThreadLocal 成员 | 私有 `handle_` + `handle()` getter | 公开 `handle` 成员 |
| Wasm 重载方法 | `rebuild(bool is_fail_recovery)` | `recover()` (已存在于 HIGRESS 分支) |
| Wasm 重载触发 | 在 FilterConfig::createFilter() 中 | 在 PluginConfig::maybeReloadHandleIfNeeded() 中 |
| 重载策略 | 简单的时间间隔控制 | 支持 `FailurePolicy::FAIL_RELOAD` + backoff |

---

## 冲突文件列表 (验证后更新)

| 序号 | 文件路径 | 冲突位置 | 冲突类型 | 风险等级 | 策略 |
|------|----------|----------|----------|----------|------|
| 1 | `source/common/http/conn_manager_impl.cc` | 行 2419-2441 | 条件编译 | 🟢 低 | 采用 Incoming |
| 2 | `source/common/listener_manager/filter_chain_manager_impl.cc` | 行 125-137 | 类型定义 | 🟢 低 | 采用 Incoming |
| 3 | `source/extensions/common/wasm/context.cc` | 行 487-494 | 新增功能 | 🟢 低 | 采用 Incoming |
| 4 | `source/extensions/common/wasm/wasm.cc` | 行 189-226 | 方法重构 | 🟡 中 | 手动合并 |
| 5 | `source/extensions/common/wasm/wasm.h` | 行 163-179 | 类结构 | 🟡 中 | 手动合并 |
| 6 | `source/extensions/filters/http/wasm/wasm_filter.h` | 行 24-95 | 架构差异 | 🟡 中 | 采用 HEAD |
| 7 | `source/extensions/filters/network/wasm/wasm_filter.h` | 行 23-82 | 架构差异 | 🟡 中 | 采用 HEAD |
| 8 | `test/common/stream_info/stream_info_impl_test.cc` | 行 43-62 | 测试断言 | 🟢 低 | 合并双方 |
| 9 | `source/common/listener_manager/listener_manager_impl.cc.orig` | 全文件 | .orig 文件 | 🟢 低 | 删除 |

---

## 详细冲突分析与解决方案

### 1. `source/common/http/conn_manager_impl.cc`

#### 冲突点 (行 2419-2441)

**冲突描述**: HIGRESS 条件编译下 `originalBufferedRequestData()` 的使用

**HEAD (1.36.4)**:
```cpp
#if defined(HIGRESS)
  // TODO(higress): In 1.36+, originalBufferedRequestData() doesn't exist.
  // Using bufferedRequestData() for now - may need to add original data tracking later.
  UNREFERENCED_PARAMETER(use_original_request_body);
#endif
```

**Incoming (patch)**:
```cpp
#if defined(HIGRESS)
  bool proxy_body = false;
  const auto& original_buffered_request_data = filter_manager_.originalBufferedRequestData();
  if (use_original_request_body && original_buffered_request_data != nullptr &&
      original_buffered_request_data->length() > 0) {
    proxy_body = true;
    request_data->move(*original_buffered_request_data);
  } else {
    // ... fallback to bufferedRequestData()
  }
  const auto& original_remote_address = ...
#else
```

**🔴 深度验证结果**:
- **HEAD 的 TODO 注释是错误的！** `originalBufferedRequestData()` API **已存在于 1.36.4**
- 在 `filter_manager.h` 第 923 行: `Buffer::InstancePtr& originalBufferedRequestData() { return original_buffered_request_data_; }`
- 成员变量 `original_buffered_request_data_` 在第 1142 行定义

**解决方案**: ✅ **采用 Incoming 版本**

API 已存在，直接使用 patch 的实现。

---

### 2. `source/common/listener_manager/filter_chain_manager_impl.cc`

#### 冲突点 (行 125-137)

**冲突描述**: `filter_chains` 变量的类型定义差异

**HEAD (1.36.4)**:
```cpp
  FilterChainsByMatcher filter_chains;
```

**Incoming (patch)**:
```cpp
#if defined(HIGRESS) && defined(ENVOY_ENABLE_FULL_PROTOS)
  absl::node_hash_map<envoy::config::listener::v3::FilterChainMatch, std::string,
                      HashCachedMessageUtil, HashCachedMessageUtil>
      filter_chains;
#else
  absl::node_hash_map<envoy::config::listener::v3::FilterChainMatch, std::string, MessageUtil,
                      MessageUtil>
      filter_chains;
#endif
```

**🔴 深度验证结果**:
- `FilterChainsByMatcher` 定义在头文件中使用 `MessageUtil`
- `HashCachedMessageUtil` **已存在于 1.36.4** (`source/common/protobuf/utility.h` 第 618 行，HIGRESS 条件下)
- Patch 方案是在局部变量级别做条件编译，覆盖类型别名

**解决方案**: ✅ **采用 Incoming 版本**

`HashCachedMessageUtil` 基础设施已存在，直接使用 patch 的条件编译方案。

---

### 3. `source/extensions/common/wasm/context.cc`

#### 冲突点 (行 487-494)

**冲突描述**: `PROPERTY_TOKENS` 宏定义，新增 `PLUGIN_VM_MEMORY`

**HEAD (1.36.4)**:
```cpp
#if defined(HIGRESS)
// (空，只有 #if 开头)
```

**Incoming (patch)**:
```cpp
#if defined(HIGRESS)
#define PROPERTY_TOKENS(_f)                                                                        \
  _f(NODE) _f(LISTENER_DIRECTION) _f(LISTENER_METADATA) _f(CLUSTER_NAME) _f(CLUSTER_METADATA)      \
      _f(ROUTE_NAME) _f(ROUTE_METADATA) _f(PLUGIN_NAME) _f(UPSTREAM_HOST_METADATA)                 \
          _f(PLUGIN_ROOT_ID) _f(PLUGIN_VM_ID) _f(PLUGIN_VM_MEMORY) _f(CONNECTION_ID)
#else
```

**分析**:
- patch 新增了 `PLUGIN_VM_MEMORY` token，用于获取 VM 内存使用量
- 这是新增功能，对现有代码无影响

**解决方案**: ✅ **采用 Incoming 版本**

```cpp
#if defined(HIGRESS)
#define PROPERTY_TOKENS(_f)                                                                        \
  _f(NODE) _f(LISTENER_DIRECTION) _f(LISTENER_METADATA) _f(CLUSTER_NAME) _f(CLUSTER_METADATA)      \
      _f(ROUTE_NAME) _f(ROUTE_METADATA) _f(PLUGIN_NAME) _f(UPSTREAM_HOST_METADATA)                 \
          _f(PLUGIN_ROOT_ID) _f(PLUGIN_VM_ID) _f(PLUGIN_VM_MEMORY) _f(CONNECTION_ID)
#else
#define PROPERTY_TOKENS(_f)                                                                        \
  _f(NODE) _f(LISTENER_DIRECTION) _f(LISTENER_METADATA) _f(CLUSTER_NAME) _f(CLUSTER_METADATA)      \
      _f(ROUTE_NAME) _f(ROUTE_METADATA) _f(PLUGIN_NAME) _f(UPSTREAM_HOST_METADATA)                 \
          _f(PLUGIN_ROOT_ID) _f(PLUGIN_VM_ID) _f(CONNECTION_ID)
#endif
```

---

### 4. `source/extensions/common/wasm/wasm.cc`

#### 冲突点 1 (行 189-197)

**冲突描述**: 方法名和签名差异

**HEAD (1.36.4)**:
```cpp
bool PluginHandleSharedPtrThreadLocal::recover() {
  if (handle == nullptr || handle->wasmHandle() == nullptr ||
      handle->wasmHandle()->wasm() == nullptr) {
```

**Incoming (patch)**:
```cpp
bool PluginHandleSharedPtrThreadLocal::rebuild(bool is_fail_recovery) {
  if (handle_ == nullptr || handle_->wasmHandle() == nullptr ||
      handle_->wasmHandle()->wasm() == nullptr) {
```

#### 冲突点 2 (行 210-226)

**HEAD (1.36.4)**:
```cpp
  if (handle->rebuild(new_handle)) {
    handle = std::static_pointer_cast<PluginHandle>(new_handle);
    handle->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
    ENVOY_LOG(info, "wasm vm recover from crash success");
```

**Incoming (patch)**:
```cpp
  if (handle_->rebuild(new_handle)) {
    handle_ = std::static_pointer_cast<PluginHandle>(new_handle);
    // Increment appropriate metrics based on rebuild type
    if (is_fail_recovery) {
      handle_->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
      ENVOY_LOG(info, "wasm vm recover from crash success");
    } else {
      handle_->wasmHandle()->wasm()->lifecycleStats().rebuild_total_.inc();
      ENVOY_LOG(info, "wasm vm rebuild success");
    }
```

**分析**:
- HEAD 使用 `handle` (公开成员)，patch 使用 `handle_` (私有成员)
- HEAD 的 `recover()` 只处理 crash recovery
- patch 的 `rebuild(bool)` 区分 crash recovery 和主动 rebuild
- patch 新增了 `rebuild_total_` 计数器

**🔴 深度验证结果**:
- 1.36.4 已有 `recover()` 实现，调用 `handle->rebuild(new_handle)` 内部方法
- patch 的 `rebuild(bool is_fail_recovery)` 是对 `recover()` 的功能增强：区分 crash recovery 和主动 rebuild
- 用户确认：**不应保留向后兼容**，直接更新为 `rebuild(bool)` 版本

**解决方案**: ✅ **将 `recover()` 替换为 `rebuild(bool)`**

保留 HEAD 的成员变量命名方式 (`handle`)，但将方法从 `recover()` 更新为 `rebuild(bool)`：

```cpp
bool PluginHandleSharedPtrThreadLocal::rebuild(bool is_fail_recovery) {
  if (handle == nullptr || handle->wasmHandle() == nullptr ||
      handle->wasmHandle()->wasm() == nullptr) {
    ENVOY_LOG(warn, "wasm has not been initialized");
    return false;
  }
  auto& dispatcher = handle->wasmHandle()->wasm()->dispatcher();
  auto now = dispatcher.timeSource().monotonicTime() + cache_time_offset_for_testing;
  if (now - last_recover_time_ < std::chrono::seconds(MIN_RECOVER_INTERVAL_SECONDS)) {
    ENVOY_LOG(info, "rebuild interval has not been reached");
    return false;
  }
  last_recover_time_ = now;
  std::shared_ptr<PluginHandleBase> new_handle;
  if (handle->rebuild(new_handle)) {
    handle = std::static_pointer_cast<PluginHandle>(new_handle);
    if (is_fail_recovery) {
      handle->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
      ENVOY_LOG(info, "wasm vm recover from crash success");
    } else {
      handle->wasmHandle()->wasm()->lifecycleStats().rebuild_total_.inc();
      ENVOY_LOG(info, "wasm vm rebuild success");
    }
    return true;
  }
  return false;
}
```

**注意**: 
1. 需要在 `stats_handler.h` 中添加 `rebuild_total` counter
2. `recover()` 方法在 1.36.4 中虽已定义但**尚未被任何地方调用**，可以安全地重命名为 `rebuild(bool)`

---

### 5. `source/extensions/common/wasm/wasm.h`

#### 冲突点 (行 163-179)

**冲突描述**: `PluginHandleSharedPtrThreadLocal` 类结构差异

**HEAD (1.36.4)**:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtr handle{};
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;

  bool recover();

private:
  MonotonicTime last_recover_time_;
};
```

**Incoming (patch)**:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr handle) : handle_(handle){};
  bool rebuild(bool is_fail_recovery = false);

  // (私有成员 handle_, last_recover_time_)
};
```

**分析**:
- HEAD 使用公开成员 `handle`，patch 使用私有成员 `handle_` + getter
- HEAD 保留了 `last_load` 成员（用于 PluginConfig 的重载逻辑）
- patch 只有 `last_recover_time_`

**🔴 深度验证结果**:
- 用户确认：**不保留 `recover()`**，直接迭代为 `rebuild(bool)` 版本
- 保留 HEAD 的公开成员 `handle` 和 `last_load`（用于 PluginConfig 的重载逻辑）

**解决方案**: ✅ **将 `recover()` 替换为 `rebuild(bool)`**

```cpp
#if defined(HIGRESS)
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtr handle{};
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;

  bool rebuild(bool is_fail_recovery = false);  // 替换原来的 recover()

private:
  MonotonicTime last_recover_time_;
};
#else
// ... 非 HIGRESS 版本保持不变
#endif
```

---

### 6. `source/extensions/filters/http/wasm/wasm_filter.h`

#### 冲突点 (行 24-95)

**冲突描述**: 架构重构差异

**HEAD (1.36.4)**:
```cpp
class FilterConfig : public Extensions::Common::Wasm::PluginConfig {
public:
  FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
               Server::Configuration::FactoryContext& context);

  FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
               Server::Configuration::UpstreamFactoryContext& context);
};
```

**Incoming (patch)**:
```cpp
class FilterConfig : Logger::Loggable<Logger::Id::wasm> {
public:
  FilterConfig(...);

  std::shared_ptr<Context> createFilter() {
    // ... 完整的 createFilter() 实现
    // 包含 shouldRebuild() 检查和 rebuild(false) 调用
  }

private:
  ThreadLocal::TypedSlotPtr<PluginHandleSharedPtrThreadLocal> tls_slot_;
  Config::DataSource::RemoteAsyncDataProviderPtr remote_data_provider_;
  Envoy::Extensions::Common::Wasm::WasmHandleSharedPtr base_wasm_handle_;
};
```

**分析**:
- 1.36.4 将 `FilterConfig` 重构为继承 `PluginConfig`
- `createFilter()` 功能已移到 `PluginConfig::createContext()` 
- `PluginConfig` 已有 `maybeReloadHandleIfNeeded()` 处理重载
- patch 的 `shouldRebuild()` 检查逻辑需要在 `PluginConfig` 层实现

**解决方案**: ✅ **采用 HEAD 版本**

保留 HEAD 版本。patch 的 `shouldRebuild()` 主动重建功能需要在 `PluginConfig::maybeReloadHandleIfNeeded()` 或 `createContext()` 中实现，这是**额外适配工作**。

---

### 7. `source/extensions/filters/network/wasm/wasm_filter.h`

#### 冲突点 (行 23-82)

**冲突描述**: 与 http/wasm_filter.h 相同的架构差异

**解决方案**: ✅ **采用 HEAD 版本**

与 http 版本相同的处理方式。

---

### 8. `test/common/stream_info/stream_info_impl_test.cc`

#### 冲突点 (行 43-62)

**冲突描述**: `StreamInfoImpl` 结构体大小断言值差异

**HEAD (1.36.4)**:
```cpp
    ASSERT_TRUE(
        // with --config=docker-msan
        sizeof(stream_info) == 728 ||
        // with --config=docker-clang
        sizeof(stream_info) == 736 ||
        // with --config=docker-clang-libc++
        sizeof(stream_info) == 704)
```

**Incoming (patch)**:
```cpp
    ASSERT_TRUE(sizeof(stream_info) == 840 || sizeof(stream_info) == 856 ||
                sizeof(stream_info) == 888 || sizeof(stream_info) == 776 ||
#if defined(HIGRESS)
                sizeof(stream_info) == 816 || sizeof(stream_info) == 768 ||
                // add hash cache to protobuf message
                sizeof(stream_info) == 784 ||
#endif
                sizeof(stream_info) == 728 || sizeof(stream_info) == 744)
```

**分析**:
- patch 添加了更多可能的 sizeof 值，因为添加了 protobuf hash 缓存
- 需要合并两边的值，确保测试覆盖所有配置

**解决方案**: 🔧 **合并双方**

```cpp
    ASSERT_TRUE(
        // with --config=docker-msan
        sizeof(stream_info) == 728 ||
        // with --config=docker-clang
        sizeof(stream_info) == 736 ||
        // with --config=docker-clang-libc++
        sizeof(stream_info) == 704 ||
        // additional sizes from various configurations
        sizeof(stream_info) == 840 || sizeof(stream_info) == 856 ||
        sizeof(stream_info) == 888 || sizeof(stream_info) == 776 ||
#if defined(HIGRESS)
        sizeof(stream_info) == 816 || sizeof(stream_info) == 768 ||
        // add hash cache to protobuf message
        sizeof(stream_info) == 784 ||
#endif
        sizeof(stream_info) == 744)
```

---

### 9. `source/common/listener_manager/listener_manager_impl.cc.orig`

**冲突描述**: `.orig` 备份文件位置冲突

**分析**:
- 这是 patch 中误包含的 `.orig` 备份文件
- 目录位置有变更（从 `source/extensions/listener_managers/` 到 `source/common/listener_manager/`）
- 不应包含在最终提交中

**解决方案**: ✅ **删除该文件**

```bash
git rm source/common/listener_manager/listener_manager_impl.cc.orig
```

---

## 解决步骤总结

### ✅ 可直接采用的冲突

| 文件 | 策略 | 说明 |
|------|------|------|
| `conn_manager_impl.cc` | 采用 Incoming | API 已存在，HEAD 的 TODO 是错误的 |
| `filter_chain_manager_impl.cc` | 采用 Incoming | `HashCachedMessageUtil` 已存在 |
| `context.cc` | 采用 Incoming | 新增 PLUGIN_VM_MEMORY |
| `http/wasm_filter.h` | 采用 HEAD | 架构已重构到 PluginConfig |
| `network/wasm_filter.h` | 采用 HEAD | 架构已重构到 PluginConfig |
| `listener_manager_impl.cc.orig` | 删除文件 | 备份文件不需要 |

### 🔧 需要手动合并

| 文件 | 说明 |
|------|------|
| `wasm.h` | 将 `recover()` 替换为 `rebuild(bool)` |
| `wasm.cc` | 将 `recover()` 替换为 `rebuild(bool)`，保留 HEAD 的 `handle` 成员访问 |
| `stream_info_impl_test.cc` | 合并两边的 sizeof 值 |

### 🔧 需要额外适配的工作

基础设施已存在，需要在此基础上完成：

1. **rebuild_total 统计指标**:
   - 在 `stats_handler.h` 的 `LIFECYCLE_STATS` 宏中添加 `PLUGIN_COUNTER(rebuild_total)`
   - 基础：`recover_total` 已存在

2. **Wasm shouldRebuild() 主动重建机制**:
   - 在 `Wasm` 类中添加 `shouldRebuild()` / `setShouldRebuild()` 方法
   - 在 `PluginConfig::createContext()` 或 `maybeReloadHandleIfNeeded()` 中检查并触发重建
   - 基础：1.36.4 已有 `maybeReloadHandleIfNeeded()` 重载逻辑

---

## 风险评估 (验证后更新)

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟡 中 | `wasm.h` / `wasm.cc` | 方法签名变更 `recover()` → `rebuild(bool)`，需更新调用方 |
| 🟡 中 | `http/wasm_filter.h` | 采用 HEAD，但需在 PluginConfig 层实现 shouldRebuild |
| 🟡 中 | `network/wasm_filter.h` | 同上 |
| 🟢 低 | `filter_chain_manager_impl.cc` | 基础设施已存在，直接采用 Incoming |
| 🟢 低 | `conn_manager_impl.cc` | API 已存在，直接采用 Incoming |
| 🟢 低 | `context.cc` | 简单的功能新增 |
| 🟢 低 | `stream_info_impl_test.cc` | 仅测试断言值 |

---

## 验证清单

- [ ] 编译通过 (`bazel build --config=clang //source/extensions/common/wasm/...`)
- [ ] 单元测试通过 (`bazel test --config=clang //test/extensions/common/wasm/...`)
- [ ] 功能验证：PLUGIN_VM_MEMORY property 可用
- [ ] 条件编译正确（HIGRESS 条件编译块正确闭合）
- [ ] 额外适配工作完成（shouldRebuild 机制）

---

## 建议的工作流程

1. **第一阶段**: 解决 cherry-pick 冲突，完成基础合并
   - 按上述方案解决 9 个冲突文件
   - 重点关注 wasm.h/wasm.cc 的正确合并

2. **第二阶段**: 完成额外适配工作
   - 在 Wasm 类添加 shouldRebuild 机制
   - 在 PluginConfig 中集成主动重建逻辑
   - 添加 rebuild_total 统计指标

3. **第三阶段**: 全面测试验证
   - 编译验证
   - 单元测试
   - 功能测试

---

**请审核以上方案，确认无误后回复"确认执行"开始解决冲突。**

如有任何判断需要进一步验证，请指出具体的条目，我将进行 Phase 4 深度验证。
