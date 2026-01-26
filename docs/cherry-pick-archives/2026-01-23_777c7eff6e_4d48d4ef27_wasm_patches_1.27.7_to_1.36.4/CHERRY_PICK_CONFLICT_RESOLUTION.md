# Cherry-pick 冲突解决方案文档

## 概述

| 项目 | 内容 |
|------|------|
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **Patch** | 000-7-wasm-trace.patch (`96a3f00b3c`) + 000-8-wasm-custom.patch (`7be55e6792`) |
| **提交信息** | Apply patch: 000-7-wasm-trace.patch, Apply patch: 000-8-wasm-custom.patch |
| **日期** | 2026-01-23 |

---

## 冲突总结

### Patch 000-7: wasm-trace (4 个冲突文件)

| 文件 | 冲突块数 | 冲突类型 | 解决策略 |
|------|---------|---------|---------|
| `envoy/stream_info/stream_info.h` | 1 | 接口新增位置 | ✅ 合并两边代码 |
| `source/common/stream_info/stream_info_impl.h` | 2 | 实现新增位置 | ✅ 合并两边代码 |
| `test/extensions/common/wasm/context_test.cc` | 1 | 测试用例新增 | ✅ 合并两边代码 |
| `test/mocks/stream_info/mocks.h` | 1 | Mock 方法新增 | ✅ 合并两边代码 |

### Patch 000-8: wasm-custom (9 个冲突文件, 18 个冲突块)

| 文件 | 冲突块数 | 冲突类型 | 解决策略 |
|------|---------|---------|---------|
| `source/extensions/common/wasm/context.cc` | 10 | 多种 | ⚠️ 需适配 |
| `source/extensions/common/wasm/context.h` | 3 | 成员/方法新增 | ✅ 合并 |
| `source/extensions/common/wasm/stats_handler.h` | 2 | 枚举/构造函数 | ✅ 合并 |
| `source/extensions/common/wasm/wasm.cc` | 1 | 事件映射 | ✅ 合并 |
| `source/extensions/common/wasm/wasm.h` | 1 | 类重构 | 🔧 需重构 |
| `source/extensions/filters/http/wasm/wasm_filter.h` | 1 | createFilter移除 | 🔧 需重构 |
| `source/extensions/filters/network/wasm/wasm_filter.h` | 1 | createFilter移除 | 🔧 需重构 |
| `test/extensions/filters/http/wasm/test_data/test_cpp.cc` | 2 | 属性列表/路径 | ✅ 合并 |
| `test/test_common/wasm_base.h` | 1 | 类型差异 | ✅ 合并 |

---

## 000-8 架构差异分析

### 重大架构变化 (1.27.7 → 1.36.4)

1. **createFilter() → createContext()**
   - 1.36 将 `createFilter()` 重构到基类 `PluginConfig::createContext()`
   - HIGRESS 的 recover 逻辑需要移到 `PluginConfig::createContext()` 中

2. **PluginHandleSharedPtrThreadLocal 重构**
   - 1.36: 公开成员 `handle`, `last_load`，默认构造函数
   - 1.27: 私有成员 `handle_`，需要 getter 方法

3. **onHeadersModified() 新增**
   - 1.36 引入 `onHeadersModified()` 抽象了 clearRouteCache 逻辑
   - 基于 ABI 版本决定是否调用 clearRouteCache()
   - HIGRESS 的 `disable_clear_route_cache_` 需要集成到此方法

4. **failure_local_reply_sent_ 新增**
   - 1.36 新增了 `failure_local_reply_sent_` 检查
   - 需要与 HIGRESS 的 `destroyed_` 检查合并

---

## 冲突详细分析

### 000-7 冲突解决方案 ✅ 已完成

#### 冲突 1: `envoy/stream_info/stream_info.h` (行 1011-1069)

**冲突原因**: 1.36.4 版本新增了多个 StreamInfo 接口方法，而 patch 在相同位置添加 HIGRESS 特定方法。

**HEAD (1.36.4) 内容**:
```cpp
// 1.36.4 新增接口
virtual bool shouldSchemeMatchUpstream() const PURE;
virtual void setShouldSchemeMatchUpstream(bool should_match_upstream) PURE;
virtual bool shouldDrainConnectionUponCompletion() const PURE;
virtual void setParentStreamInfo(const StreamInfo& parent_stream_info) PURE;
virtual OptRef<const StreamInfo> parentStreamInfo() const PURE;
virtual void clearParentStreamInfo() PURE;
virtual void setShouldDrainConnectionUponCompletion(bool should_drain) PURE;
```

**Patch (000-7) 内容**:
```cpp
#ifdef HIGRESS
virtual void setCustomSpanTag(std::string_view key, std::string_view value) PURE;
virtual const absl::flat_hash_map<std::string, std::string>& getCustomSpanTagMap() const PURE;
#endif
```

**解决方案**: ✅ 保留 HEAD 的所有方法，在末尾添加 Patch 的 HIGRESS 块。

```cpp
// 保留 HEAD 的所有方法
virtual bool shouldSchemeMatchUpstream() const PURE;
virtual void setShouldSchemeMatchUpstream(bool should_match_upstream) PURE;
virtual bool shouldDrainConnectionUponCompletion() const PURE;
virtual void setParentStreamInfo(const StreamInfo& parent_stream_info) PURE;
virtual OptRef<const StreamInfo> parentStreamInfo() const PURE;
virtual void clearParentStreamInfo() PURE;
virtual void setShouldDrainConnectionUponCompletion(bool should_drain) PURE;

// 添加 Patch 的 HIGRESS 块
#ifdef HIGRESS
virtual void setCustomSpanTag(std::string_view key, std::string_view value) PURE;
virtual const absl::flat_hash_map<std::string, std::string>& getCustomSpanTagMap() const PURE;
#endif
```

---

#### 冲突 2: `source/common/stream_info/stream_info_impl.h` (行 452-487)

**冲突原因**: 1.36.4 版本新增了多个接口实现，patch 在相同位置添加 HIGRESS 实现。

**HEAD (1.36.4) 内容**:
```cpp
bool shouldSchemeMatchUpstream() const override { return should_scheme_match_upstream_; }
void setShouldSchemeMatchUpstream(bool should_match_upstream) override { ... }
bool shouldDrainConnectionUponCompletion() const override { return should_drain_connection_; }
void setShouldDrainConnectionUponCompletion(bool should_drain) override { ... }
void setParentStreamInfo(const StreamInfo& parent_stream_info) override { ... }
OptRef<const StreamInfo> parentStreamInfo() const override { return parent_stream_info_; }
void clearParentStreamInfo() override { parent_stream_info_.reset(); }
```

**Patch (000-7) 内容**:
```cpp
#ifdef HIGRESS
void setCustomSpanTag(std::string_view key, std::string_view value) override { ... }
const absl::flat_hash_map<std::string, std::string>& getCustomSpanTagMap() const override { ... }
#endif
```

**解决方案**: ✅ 保留 HEAD 的所有实现，在末尾添加 Patch 的 HIGRESS 块。

---

#### 冲突 3: `source/common/stream_info/stream_info_impl.h` (行 530-545)

**冲突原因**: 1.36.4 版本新增了多个成员变量，patch 添加 HIGRESS 成员变量。

**HEAD (1.36.4) 内容**:
```cpp
OptRef<const StreamInfo> parent_stream_info_;
uint64_t bytes_received_{};
uint64_t bytes_retransmitted_{};
uint64_t packets_retransmitted_{};
uint64_t bytes_sent_{};
Tracing::Reason trace_reason_;
bool health_check_request_{};
bool should_scheme_match_upstream_{false};
bool should_drain_connection_{false};
bool is_shadow_{false};
```

**Patch (000-7) 内容**:
```cpp
#ifdef HIGRESS
absl::flat_hash_map<std::string, std::string> custom_span_tags_;
#endif
```

**解决方案**: ✅ 保留 HEAD 的所有成员变量，在末尾添加 Patch 的 HIGRESS 块。

---

#### 冲突 4: `test/extensions/common/wasm/context_test.cc` (行 257-342)

**冲突原因**: 1.36.4 版本新增了三个测试用例，patch 添加一个 HIGRESS 测试用例。

**HEAD (1.36.4) 测试用例**:
- `ClearRouteCacheCalledInDownstreamConfigurationForLegacyWasmPlugin`
- `NoAutoClearRouteCacheCalledInDownstreamConfiguration`
- `ClearRouteCacheDoesNothingInUpstreamConfiguration`

**Patch (000-7) 测试用例**:
- `SetCustomSpanTagTest` (在 HIGRESS 宏中)

**解决方案**: ✅ 保留 HEAD 的所有测试用例，在末尾添加 Patch 的 HIGRESS 测试。

---

#### 冲突 5: `test/mocks/stream_info/mocks.h` (行 172-187)

**冲突原因**: 1.36.4 版本新增了多个 mock 方法，patch 添加 HIGRESS mock 方法。

**HEAD (1.36.4) Mock 方法**:
```cpp
MOCK_METHOD(bool, shouldSchemeMatchUpstream, (), (const));
MOCK_METHOD(void, setShouldSchemeMatchUpstream, (bool));
MOCK_METHOD(bool, shouldDrainConnectionUponCompletion, (), (const));
MOCK_METHOD(void, setShouldDrainConnectionUponCompletion, (bool));
MOCK_METHOD(void, setParentStreamInfo, (const StreamInfo&), ());
MOCK_METHOD(void, clearParentStreamInfo, ());
MOCK_METHOD(OptRef<const StreamInfo>, parentStreamInfo, (), (const));
MOCK_METHOD(void, addCustomFlag, (absl::string_view));
MOCK_METHOD(absl::string_view, customFlags, (), (const));
```

**Patch (000-7) Mock 方法**:
```cpp
#ifdef HIGRESS
MOCK_METHOD(void, setCustomSpanTag, (absl::string_view, absl::string_view));
MOCK_METHOD((const absl::flat_hash_map<std::string, std::string>&), getCustomSpanTagMap, (), (const));
#endif
```

**解决方案**: ✅ 保留 HEAD 的所有 mock 方法，在末尾添加 Patch 的 HIGRESS 块。

---

### 000-8 冲突解决方案 ✅ 已完成

#### 冲突 1: `source/extensions/common/wasm/context.cc` (行 74-100)

**冲突原因**: HEAD 新增 `DefaultAllowOnHeadersStopIteration` 常量，Patch 添加 HIGRESS 特定常量和辅助函数。

**HEAD (1.36.4) 内容**:
```cpp
constexpr bool DefaultAllowOnHeadersStopIteration = false;
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
constexpr absl::string_view CustomeTraceSpanTagPrefix = "trace_span_tag.";
constexpr std::string_view ClearRouteCacheKey = "clear_route_cache";
constexpr std::string_view DisableClearRouteCache = "off";
constexpr std::string_view SetDecoderBufferLimit = "set_decoder_buffer_limit";
constexpr std::string_view SetEncoderBufferLimit = "set_encoder_buffer_limit";

bool stringViewToUint32(std::string_view str, uint32_t& out_value) { ... }
#endif
```

**解决方案**: ✅ 保留 HEAD 的常量，在其后添加 Patch 的 HIGRESS 块。

---

#### 冲突 2: `source/extensions/common/wasm/context.cc` (行 558-630)

**冲突原因**: HEAD 为空（只有 break），Patch 添加了多个 PropertyToken case 处理。

**Patch (000-8) 新增的 case**:
- `PropertyToken::NODE`
- `PropertyToken::LISTENER_DIRECTION`
- `PropertyToken::LISTENER_METADATA`
- `PropertyToken::CLUSTER_NAME`
- `PropertyToken::CLUSTER_METADATA`
- `PropertyToken::UPSTREAM_HOST_METADATA`
- `PropertyToken::ROUTE_NAME` (带 HIGRESS 条件分支)
- `PropertyToken::ROUTE_METADATA`

**解决方案**: ✅ 添加 Patch 的所有新 case，因为 1.36 中这些 case 不存在。

---

#### 冲突 3-6: `source/extensions/common/wasm/context.cc` (行 799-947)

**冲突原因**: 1.36 新增 `onHeadersModified(type)` 方法抽象了 clearRouteCache 逻辑，而 Patch 直接在每个方法中调用 clearRouteCache。

**HEAD (1.36.4) 内容**:
```cpp
onHeadersModified(type);  // 抽象方法，内部会检查 ABI 版本
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
if (type == WasmHeaderMapType::RequestHeaders && decoder_callbacks_ && !disable_clear_route_cache_) {
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}
#else
if (type == WasmHeaderMapType::RequestHeaders && decoder_callbacks_) {
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}
#endif
```

**解决方案**: ✅ 保留 HEAD 的 `onHeadersModified(type)` 调用。HIGRESS 的 `disable_clear_route_cache_` 逻辑已在 `context.h` 的 `clearRouteCache()` 方法中实现。

涉及的 4 个方法:
- `addHeaderMapValue()`
- `setHeaderMapPairs()`
- `removeHeaderMapValue()`
- `replaceHeaderMapValue()`

---

#### 冲突 7-10: `source/extensions/common/wasm/context.cc` (行 2074-2174)

**冲突原因**: 1.36 新增 `failure_local_reply_sent_` 检查，Patch 使用 HIGRESS 条件的 `destroyed_` 检查。

**HEAD (1.36.4) 内容**:
```cpp
if (!in_vm_context_created_ || failure_local_reply_sent_) {
  return Http::FilterHeadersStatus::Continue;
}
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
if (destroyed_ || !in_vm_context_created_) {
#else
if (!in_vm_context_created_) {
#endif
```

**解决方案**: ✅ 合并两边条件，HIGRESS 下检查所有三个条件：

```cpp
#if defined(HIGRESS)
if (destroyed_ || !in_vm_context_created_ || failure_local_reply_sent_) {
#else
if (!in_vm_context_created_ || failure_local_reply_sent_) {
#endif
```

涉及的 4 个方法:
- `encodeHeaders()`
- `encodeData()`
- `encodeTrailers()`
- `encodeMetadata()`

---

#### 冲突 11: `source/extensions/common/wasm/context.h` (行 220-228)

**冲突原因**: HEAD 增加了 `downstreamCallbacks()` 空指针检查，Patch 添加 HIGRESS 的 `disable_clear_route_cache_` 检查。

**HEAD (1.36.4) 内容**:
```cpp
if (decoder_callbacks_ && decoder_callbacks_->downstreamCallbacks()) {
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
if (decoder_callbacks_ && !disable_clear_route_cache_) {
#else
if (decoder_callbacks_) {
#endif
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}
```

**解决方案**: ✅ 合并两边条件，HIGRESS 下检查所有三个条件：

```cpp
#if defined(HIGRESS)
if (decoder_callbacks_ && decoder_callbacks_->downstreamCallbacks() && !disable_clear_route_cache_) {
#else
if (decoder_callbacks_ && decoder_callbacks_->downstreamCallbacks()) {
#endif
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}
```

---

#### 冲突 12: `source/extensions/common/wasm/context.h` (行 466-477)

**冲突原因**: HEAD 和 Patch 在成员变量初始化格式上有差异，Patch 添加 HIGRESS 的 `redis_call_response_`。

**HEAD (1.36.4) 内容**:
```cpp
Http::HeaderMapPtr grpc_receive_initial_metadata_;
Http::HeaderMapPtr grpc_receive_trailing_metadata_;
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
std::string redis_call_response_{};
#endif

Http::HeaderMapPtr grpc_receive_initial_metadata_{};
Http::HeaderMapPtr grpc_receive_trailing_metadata_{};
```

**解决方案**: ✅ 添加 HIGRESS 的 `redis_call_response_`，保留 HEAD 的初始化格式（不带 `{}`）：

```cpp
#if defined(HIGRESS)
std::string redis_call_response_{};
#endif

Http::HeaderMapPtr grpc_receive_initial_metadata_;
Http::HeaderMapPtr grpc_receive_trailing_metadata_;
```

---

#### 冲突 13: `source/extensions/common/wasm/context.h` (行 518-526)

**冲突原因**: HEAD 新增 `abi_version_` 和 `allow_on_headers_stop_iteration_`，Patch 添加 HIGRESS 的 `disable_clear_route_cache_`。

**HEAD (1.36.4) 内容**:
```cpp
proxy_wasm::AbiVersion abi_version_{proxy_wasm::AbiVersion::Unknown};
bool allow_on_headers_stop_iteration_{false};
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
bool disable_clear_route_cache_ = false;
#endif
```

**解决方案**: ✅ 保留 HEAD 的成员变量，在末尾添加 HIGRESS 块：

```cpp
proxy_wasm::AbiVersion abi_version_{proxy_wasm::AbiVersion::Unknown};
bool allow_on_headers_stop_iteration_{false};
#if defined(HIGRESS)
bool disable_clear_route_cache_ = false;
#endif
```

---

#### 冲突 14: `source/extensions/common/wasm/stats_handler.h` (行 75-83)

**冲突原因**: HEAD 新增 `VmReloadBackoff`, `VmReloadSuccess`, `VmReloadFailure` 枚举值，Patch 添加 HIGRESS 的 `RecoverError`。

**HEAD (1.36.4) 内容**:
```cpp
VmReloadBackoff,
VmReloadSuccess,
VmReloadFailure,
```

**Patch (000-8) 内容**:
```cpp
#ifdef HIGRESS
RecoverError,
#endif
```

**解决方案**: ✅ 保留 HEAD 的所有枚举值，在末尾添加 HIGRESS 块：

```cpp
VmReloadBackoff,
VmReloadSuccess,
VmReloadFailure,
#ifdef HIGRESS
RecoverError,
#endif
```

---

#### 冲突 15: `source/extensions/common/wasm/stats_handler.h` (行 130-135)

**冲突原因**: 构造函数初始化列表的格式差异（`{});` vs `{}){}`）和多余的 `#endif`。

**解决方案**: ✅ 保留 HEAD 的格式，添加缺失的 `#endif` 以正确关闭 `#else` 块。

---

#### 冲突 16: `source/extensions/common/wasm/wasm.cc` (行 368-376)

**冲突原因**: HEAD 将 `RecoverError` 映射到 `RuntimeError`，Patch 在 HIGRESS 下映射到 `RecoverError`。

**HEAD (1.36.4) 内容**:
```cpp
case FailState::RecoverError:
  return WasmEvent::RuntimeError;
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
case FailState::RecoverError:
  return WasmEvent::RecoverError;
#endif
```

**解决方案**: ✅ 使用条件编译，HIGRESS 映射到 `RecoverError`，否则映射到 `RuntimeError`：

```cpp
#if defined(HIGRESS)
case FailState::RecoverError:
  return WasmEvent::RecoverError;
#else
case FailState::RecoverError:
  return WasmEvent::RuntimeError;
#endif
```

---

#### 冲突 17: `source/extensions/common/wasm/wasm.h` (行 168-185)

**冲突原因**: 1.36 重构了 `PluginHandleSharedPtrThreadLocal` 类架构。

**HEAD (1.36.4) 架构**:
- 公开成员 `handle`, `last_load`
- 支持 move 语义的构造函数
- 有默认构造函数

**Patch (000-8) 架构**:
- 私有成员 `handle_`，需要 getter 方法
- HIGRESS 有 `recover()` 方法和 `last_recover_time_`

**解决方案**: 🔧 为 HIGRESS 重新设计类以适配 1.36 架构：

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

  bool recover();

private:
  MonotonicTime last_recover_time_;
};
#else
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject {
public:
  PluginHandleSharedPtr handle{};
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;
};
#endif
```

---

#### 冲突 18: `source/extensions/filters/http/wasm/wasm_filter.h` (行 24-83)

**冲突原因**: 1.36 将 `createFilter()` 重构到基类 `PluginConfig::createContext()`。

**HEAD (1.36.4) 内容**:
```cpp
FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
             Server::Configuration::UpstreamFactoryContext& context);
```

**Patch (000-8) 内容**: 完整的 `createFilter()` 方法实现（包含 HIGRESS recover 逻辑）。

**解决方案**: 🔧 保留 HEAD 的构造函数声明，不添加 `createFilter()`。
HIGRESS 的 recover 逻辑需要后续移到 `PluginConfig::createContext()` 中（在 `wasm.cc`）。

---

#### 冲突 19: `source/extensions/filters/network/wasm/wasm_filter.h` (行 23-82)

**冲突原因**: 同冲突 18，`createFilter()` 被移到基类。

**解决方案**: 🔧 保留 HEAD 的结构，不添加 `createFilter()`。

---

#### 冲突 20: `test/extensions/filters/http/wasm/test_data/test_cpp.cc` (行 67-81)

**冲突原因**: 属性列表格式差异。

**HEAD (1.36.4) 内容**:
```cpp
const std::vector<std::string> properties = {
    "string_state",     "metadata",   "request",        "response",    "connection",
    "connection_id",    "upstream",   "source",         "destination", "cluster_name",
    "cluster_metadata", "route_name", "route_metadata", "upstream_host_metadata",
    "filter_state", "listener_direction" ,"listener_metadata",
};
```

**Patch (000-8) 内容**: 多行格式，缺少 `listener_direction` 和 `listener_metadata`。

**解决方案**: ✅ 保留 HEAD 的列表（已包含所有属性）。

---

#### 冲突 21: `test/extensions/filters/http/wasm/test_data/test_cpp.cc` (行 470-475)

**冲突原因**: 1.36 更改了属性路径。

**HEAD (1.36.4) 内容**:
```cpp
getValue({"xds", "upstream_host_metadata", "filter_metadata", "namespace", "key"}, ...)
```

**Patch (000-8) 内容**:
```cpp
getValue({"upstream_host_metadata", "filter_metadata", "namespace", "key"}, ...)
```

**解决方案**: ✅ 保留 HEAD 的路径（1.36 API 变化）。

---

#### 冲突 22: `test/test_common/wasm_base.h` (行 157-173)

**冲突原因**: HEAD 使用 `std::shared_ptr<Context>`，Patch 使用 `std::unique_ptr<Context>` 并添加 HIGRESS 的 `doRecover()` 方法。

**HEAD (1.36.4) 内容**:
```cpp
std::shared_ptr<Context> context_;
```

**Patch (000-8) 内容**:
```cpp
#if defined(HIGRESS)
template <typename TestFilter> void doRecover() { ... }
#endif

std::unique_ptr<Context> context_;
```

**解决方案**: ✅ 添加 HIGRESS 的 `doRecover()` 方法，保留 HEAD 的 `std::shared_ptr` 类型：

```cpp
#if defined(HIGRESS)
template <typename TestFilter> void doRecover() { ... }
#endif

std::shared_ptr<Context> context_;
```

---

## 风险评估

| 风险项 | 级别 | 说明 |
|--------|------|------|
| ⚠️ 接口签名差异 | **高** | **需适配**: Patch 使用 `std::string_view`，但 1.36.4 统一使用 `absl::string_view` |
| 编译验证 | 中 | 修改签名后需要编译验证 |
| 测试适配 | 低 | Mock 方法签名需要与接口保持一致 |

### ⚠️ 需要适配: string_view 类型

**原始 patch (1.27.7)**:
```cpp
virtual void setCustomSpanTag(std::string_view key, std::string_view value) PURE;
```

**适配后 (1.36.4)**:
```cpp
virtual void setCustomSpanTag(absl::string_view key, absl::string_view value) PURE;
```

**原因**: 1.36.4 中 StreamInfo 接口统一使用 `absl::string_view`，需要保持一致性。

---

## 验证清单

- [ ] 所有 HIGRESS 宏块正确添加
- [ ] 接口签名与实现一致
- [ ] Mock 方法签名与接口一致
- [ ] 编译通过
- [ ] 相关测试通过

---

## 状态

### 000-7 状态: ✅ 已完成
- Cherry-pick commit: `777c7eff6e`
- 所有冲突已解决 (5 个冲突块)
- 已适配 `std::string_view` → `absl::string_view`

### 000-8 状态: ✅ 已完成
- Cherry-pick commit: `14cb7c658a`
- 所有冲突已解决 (22 个冲突块)
- 架构适配:
  - `onHeadersModified()` 方法保留，HIGRESS 逻辑集成到 `clearRouteCache()`
  - `PluginHandleSharedPtrThreadLocal` 适配 1.36 公开成员架构
  - `createFilter()` 已移除（由基类 `PluginConfig::createContext()` 处理）
  - 条件检查合并 `destroyed_` + `failure_local_reply_sent_`

### 待处理
- [ ] Phase 7: 编译验证
- [ ] Phase 8: 单元测试验证
- [ ] Phase 9: 文档归档

### ⚠️ 注意事项
1. **HIGRESS recover 逻辑**: 原来在 `wasm_filter.h` 的 `createFilter()` 中的 recover 逻辑需要移到 `wasm.cc` 的 `PluginConfig::createContext()` 中
2. **PluginHandleSharedPtrThreadLocal::recover()**: 需要在 `wasm.cc` 中实现此方法
