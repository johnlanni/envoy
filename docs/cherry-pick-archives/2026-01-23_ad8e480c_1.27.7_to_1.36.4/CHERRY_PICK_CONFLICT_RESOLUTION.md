# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `ad8e480c84` - Apply patch: 000-4-custom-response-fallback.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 6 个

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `envoy/http/filter.h` | 行 849-864 | 接口签名差异 + HIGRESS 功能 |
| 2 | `source/common/http/filter_manager.cc` | 行 475-480 | 方法定义 + HIGRESS 条件编译 |
| 3 | `source/common/http/filter_manager.h` | 行 296-306, 313-326, 1152-1159 | 类型差异 + 接口差异 + HIGRESS 功能 |
| 4 | `source/common/router/router.cc` | 行 988-1019 | 缓冲区逻辑改进 + HIGRESS 功能 |
| 5 | `source/extensions/filters/http/custom_response/custom_response_filter.cc` | 行 80-111 | API 差异 + HIGRESS 功能 |
| 6 | `test/mocks/http/mocks.h` | 行 283-290 | Mock 方法差异 |

---

## 🔴 架构差异警告

### 1.27 → 1.36 关键接口变化

| 组件 | 1.27.7 (源分支) | 1.36.4 (目标分支) |
|------|-----------------|-------------------|
| `upstreamOverrideHost()` 返回类型 | `absl::optional<absl::string_view>` | `absl::optional<Upstream::LoadBalancerContext::OverrideHost>` |
| `setUpstreamOverrideHost()` 参数 | `absl::string_view` | `Upstream::LoadBalancerContext::OverrideHost` |
| `decoderBufferLimit()` 返回类型 | `uint32_t` | `uint64_t` |
| `buffer_limit_` 类型 | `uint32_t` | `uint64_t` |
| 缓冲区限制变量名 | `retry_shadow_buffer_limit_` | `effective_buffer_limit` (计算得到) |
| 配置遍历 API | `traversePerFilterConfig()` | `getAllPerFilterConfig<T>()` |
| 新增方法 (1.36) | 无 | `shouldLoadShed()`, `sendGoAwayAndClose()` |

---

## 详细冲突分析与解决方案

### 1. `envoy/http/filter.h`

#### 冲突点 1 (行 849-864)

**冲突描述**: 接口方法签名差异 + HIGRESS 新增方法

- **HEAD (1.36.4)**: 
  - `upstreamOverrideHost()` 返回 `absl::optional<Upstream::LoadBalancerContext::OverrideHost>`
  - 有 `shouldLoadShed()` 方法
  
- **Incoming (1.27.7 patch)**: 
  - `upstreamOverrideHost()` 返回 `absl::optional<absl::string_view>`
  - HIGRESS 条件编译: `needBuffering()` 和 `setNeedBuffering()` 方法

**解决方案**: 合并双方 - 保留 HEAD 的接口签名和 `shouldLoadShed()`，添加 HIGRESS 条件编译的方法

```cpp
  virtual absl::optional<Upstream::LoadBalancerContext::OverrideHost>
  upstreamOverrideHost() const PURE;

  /**
   * @return true if the filter should shed load based on the system pressure, typically memory.
   */
  virtual bool shouldLoadShed() const PURE;

#if defined(HIGRESS)
  virtual bool needBuffering() const { return false; }
  virtual void setNeedBuffering(bool) {}
#endif
```

---

### 2. `source/common/http/filter_manager.cc`

#### 冲突点 1 (行 475-480)

**冲突描述**: HEAD 有 `shouldLoadShed()` 方法定义，Incoming 开始 HIGRESS 条件编译块

- **HEAD (1.36.4)**: 
  ```cpp
  bool ActiveStreamDecoderFilter::shouldLoadShed() const { return parent_.shouldLoadShed(); }
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  #if defined(HIGRESS)
  ```

**解决方案**: 合并双方 - 保留 HEAD 的方法定义，添加 HIGRESS 条件编译

```cpp
bool ActiveStreamDecoderFilter::shouldLoadShed() const { return parent_.shouldLoadShed(); }

#if defined(HIGRESS)
```

---

### 3. `source/common/http/filter_manager.h`

#### 冲突点 1 (行 296-306)

**冲突描述**: 类型差异 (uint32_t vs uint64_t) + HIGRESS 新增重载方法

- **HEAD (1.36.4)**:
  ```cpp
  void setDecoderBufferLimit(uint64_t limit) override;
  uint64_t decoderBufferLimit() override;
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  void setDecoderBufferLimit(uint32_t limit) override;
  uint32_t decoderBufferLimit() override;
  #if defined(HIGRESS)
    bool recreateStream(const Http::ResponseHeaderMap* original_response_headers,
                        bool use_original_request_body) override;
  #endif
  ```

**解决方案**: 保留 HEAD 的 uint64_t 类型，添加 HIGRESS 条件编译的重载方法

```cpp
  void setDecoderBufferLimit(uint64_t limit) override;
  uint64_t decoderBufferLimit() override;
#if defined(HIGRESS)
  bool recreateStream(const Http::ResponseHeaderMap* original_response_headers,
                      bool use_original_request_body) override;
#endif
```

#### 冲突点 2 (行 313-326)

**冲突描述**: 接口签名差异 + HEAD 新增方法 + HIGRESS 方法

- **HEAD (1.36.4)**:
  ```cpp
  void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost) override;
  absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const override;
  bool shouldLoadShed() const override;
  void sendGoAwayAndClose() override;
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  void setUpstreamOverrideHost(absl::string_view host) override;
  absl::optional<absl::string_view> upstreamOverrideHost() const override;
  #if defined(HIGRESS)
    bool needBuffering() const override { return need_buffering_; }
    void setNeedBuffering(bool need) override { need_buffering_ = need; }
  #endif
  ```

**解决方案**: 保留 HEAD 的接口签名和新方法，添加 HIGRESS 条件编译的方法

```cpp
  void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost) override;
  absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const override;
  bool shouldLoadShed() const override;
  void sendGoAwayAndClose() override;
#if defined(HIGRESS)
  bool needBuffering() const override { return need_buffering_; }
  void setNeedBuffering(bool need) override { need_buffering_ = need; }
#endif
```

#### 冲突点 3 (行 1152-1159)

**冲突描述**: 类型差异 + HIGRESS 新增成员变量

- **HEAD (1.36.4)**:
  ```cpp
  uint64_t buffer_limit_{0};
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  #if defined(HIGRESS)
    Buffer::InstancePtr original_buffered_request_data_;
  #endif
  uint32_t buffer_limit_{0};
  ```

**解决方案**: 保留 HEAD 的 uint64_t 类型，添加 HIGRESS 条件编译的成员变量

```cpp
#if defined(HIGRESS)
  Buffer::InstancePtr original_buffered_request_data_;
#endif
  uint64_t buffer_limit_{0};
```

---

### 4. `source/common/router/router.cc`

#### 冲突点 1 (行 988-1019)

**冲突描述**: 1.36 版本有改进的缓冲区溢出处理逻辑，HIGRESS 需要在 buffering 条件中添加 `needBuffering()`

- **HEAD (1.36.4)**: 改进的缓冲区溢出处理，使用计算的 `effective_buffer_limit`，有更详细的条件判断
  
- **Incoming (1.27.7 patch)**: HIGRESS 条件编译添加 `callbacks_->needBuffering()` 到 buffering 条件

**解决方案**: 保留 HEAD 的改进逻辑，在 HIGRESS 条件编译中添加 `callbacks_->needBuffering()` 到 buffering 条件

```cpp
#if defined(HIGRESS)
  bool retry_enabled = retry_state_ && retry_state_->enabled();
  bool redirect_enabled = route_entry_ && route_entry_->internalRedirectPolicy().enabled();
  bool buffering = retry_enabled || redirect_enabled || callbacks_->needBuffering();
#else
  bool retry_enabled = retry_state_ && retry_state_->enabled();
  bool redirect_enabled = route_entry_ && route_entry_->internalRedirectPolicy().enabled();
  bool buffering = retry_enabled || redirect_enabled;
#endif
  uint64_t effective_buffer_limit = calculateEffectiveBufferLimit();

  // Check if we would exceed buffer limits, regardless of current buffering state
  // This ensures error details are set even if retry state was cleared due to upstream reset.
  bool would_exceed_buffer =
      (getLength(callbacks_->decodingBuffer()) + data.length() > effective_buffer_limit);

  // Handle retry/shadow buffer overflow, excluding redirect-only scenarios.
  // For redirect scenarios, buffer overflow should only affect redirect processing, not initial
  // request.
  bool had_retry_or_shadow = retry_enabled;
  bool is_redirect_only = redirect_enabled && !retry_enabled;

  if (would_exceed_buffer && had_retry_or_shadow && !is_redirect_only &&
      !request_buffer_overflowed_) {
```

---

### 5. `source/extensions/filters/http/custom_response/custom_response_filter.cc`

#### 冲突点 1 (行 80-111)

**冲突描述**: API 差异 - 1.36 使用新的 `getAllPerFilterConfig<T>()` API，1.27 使用 `traversePerFilterConfig()`

- **HEAD (1.36.4)**: 
  ```cpp
  for (const FilterConfig& typed_config :
       Http::Utility::getAllPerFilterConfig<FilterConfig>(encoder_callbacks_)) {
    auto maybe_policy = typed_config.getPolicy(headers, encoder_callbacks_->streamInfo());
    if (maybe_policy) {
      policy = maybe_policy;
    }
  }
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  #if defined(HIGRESS)
    if (has_rules_) {
  #endif
      decoder_callbacks_->traversePerFilterConfig(
          [&policy, &headers, this](const Router::RouteSpecificFilterConfig& config) {
            const FilterConfig* typed_config = dynamic_cast<const FilterConfig*>(&config);
            if (typed_config) {
              auto maybe_policy = typed_config->getPolicy(headers, encoder_callbacks_->streamInfo());
              if (maybe_policy) {
                policy = maybe_policy;
              }
            }
          });
  #if defined(HIGRESS)
    }
  #endif
  ```

**解决方案**: 保留 HEAD 的新 API，在 HIGRESS 条件编译中添加 `has_rules_` 检查

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

---

### 6. `test/mocks/http/mocks.h`

#### 冲突点 1 (行 283-290)

**冲突描述**: HEAD 有 `sendGoAwayAndClose()` mock，HIGRESS 有 `recreateStream` 重载 mock

- **HEAD (1.36.4)**:
  ```cpp
  MOCK_METHOD(void, sendGoAwayAndClose, ());
  ```

- **Incoming (1.27.7 patch)**:
  ```cpp
  #if defined(HIGRESS)
    MOCK_METHOD(bool, recreateStream,
                (const ResponseHeaderMap* headers, bool use_original_request_body));
  #endif
  ```

**解决方案**: 合并双方

```cpp
  MOCK_METHOD(void, sendGoAwayAndClose, ());
#if defined(HIGRESS)
  MOCK_METHOD(bool, recreateStream,
              (const ResponseHeaderMap* headers, bool use_original_request_body));
#endif
```

---

## ⚠️ 需要检查的额外适配

### 1. HIGRESS `need_buffering_` 成员变量

在 `ActiveStreamDecoderFilter` 类中需要添加 `need_buffering_` 成员变量（用于 HIGRESS 条件编译）。

### 2. `recreateStream` 重载实现

`recreateStream(const Http::ResponseHeaderMap*, bool use_original_request_body)` 的实现需要适配到目标分支。

---

## 解决步骤总结

### ✅ 可直接合并的冲突

以下冲突可以通过简单的合并策略解决:
- `envoy/http/filter.h` - 合并双方，保留 HEAD 签名 + HIGRESS 方法
- `source/common/http/filter_manager.cc` - 合并双方
- `source/common/http/filter_manager.h` - 合并双方，保留 HEAD 类型 + HIGRESS 功能
- `test/mocks/http/mocks.h` - 合并双方

### ⚠️ 需要仔细手动处理

以下文件需要特别注意:
1. `source/common/router/router.cc` - 需要将 HIGRESS 的 `needBuffering()` 整合到 HEAD 的改进逻辑中
2. `source/extensions/filters/http/custom_response/custom_response_filter.cc` - 需要适配 1.36 的新 API

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟡 中 | `source/common/router/router.cc` | 缓冲区逻辑有重大改进，需仔细整合 |
| 🟡 中 | `custom_response_filter.cc` | API 变化，需验证功能正确性 |
| 🟢 低 | `filter.h` | 纯接口声明，风险较低 |
| 🟢 低 | `filter_manager.h` | 类型兼容，添加条件编译 |
| 🟢 低 | `filter_manager.cc` | 简单合并 |
| 🟢 低 | `mocks.h` | 测试代码，不影响生产 |

---

## 验证清单

- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 功能验证通过
- [ ] 条件编译正确（HIGRESS）
- [ ] 接口类型一致性检查
- [ ] 新增成员变量初始化检查

---

## 建议的执行顺序

1. **第一步**: 解决 `envoy/http/filter.h` (接口定义)
2. **第二步**: 解决 `source/common/http/filter_manager.h` (类定义)
3. **第三步**: 解决 `source/common/http/filter_manager.cc` (实现)
4. **第四步**: 解决 `source/common/router/router.cc` (路由逻辑)
5. **第五步**: 解决 `source/extensions/filters/http/custom_response/custom_response_filter.cc` (过滤器)
6. **第六步**: 解决 `test/mocks/http/mocks.h` (测试)
