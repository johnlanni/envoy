# Cherry-pick 冲突解决方案

## 概述

| 项目 | 内容 |
|------|------|
| **原始提交** | `012aa7596e13ea3301fba10a3fb532bbfaa8b064` |
| **提交信息** | Apply patch: 015-mcp-sse-stateful-session-contrib.patch |
| **源分支** | 1.27.7-merge |
| **目标分支** | 1.36.4-merge |
| **冲突文件数** | 17 |
| **新增文件数** | 28 (无冲突，自动添加) |

## Patch 功能说明

此 patch 添加了 `mcp_sse_stateful_session` 扩展，用于 MCP SSE（Model Context Protocol Server-Sent Events）的有状态会话支持。

## 冲突文件列表

| # | 文件路径 | 冲突类型 | 风险等级 |
|---|---------|---------|---------|
| 1 | `CODEOWNERS` | 配置合并 | 🟢 低 |
| 2 | `api/BUILD` | 配置合并 | 🟢 低 |
| 3 | `api/versioning/BUILD` | 配置合并 | 🟢 低 |
| 4 | `contrib/contrib_build_config.bzl` | 配置合并 | 🟢 低 |
| 5 | `contrib/extensions_metadata.yaml` | 配置合并 | 🟢 低 |
| 6 | `envoy/http/filter.h` | 接口差异 | 🟡 中 |
| 7 | `envoy/upstream/load_balancer.h` | 注释差异 | 🟢 低 |
| 8 | `source/common/http/async_client_impl.h` | 接口差异 | 🟡 中 |
| 9 | `source/common/http/filter_manager.cc` | 接口差异 | 🟡 中 |
| 10 | `source/common/http/filter_manager.h` | 接口差异 | 🟡 中 |
| 11 | `source/extensions/common/wasm/context.cc` | 接口差异 | 🟡 中 |
| 12 | `source/extensions/filters/http/stateful_session/stateful_session.cc` | 逻辑差异 | 🟠 高 |
| 13 | `test/common/http/filter_manager_test.cc` | 测试值差异 | 🟡 中 |
| 14 | `test/extensions/filters/http/stateful_session/stateful_session_test.cc` | 测试断言差异 | 🟡 中 |
| 15 | `test/mocks/http/mocks.cc` | 格式差异 | 🟢 低 |
| 16 | `test/mocks/http/mocks.h` | 接口差异 | 🟡 中 |
| 17 | `tools/extensions/extensions_schema.yaml` | 配置合并 | 🟢 低 |

---

## 冲突详细分析与解决方案

### 1. CODEOWNERS

**冲突位置**: 文件末尾

**冲突内容**:
```
<<<<<<< HEAD
/contrib/generic_proxy/ @wbpcode @UNOWNED
/contrib/tap_sinks/ @coolg92003 @yiyibaoguo
=======
/contrib/generic_proxy/ @wbpcode @soulxu @zhaohuabing @rojkov @htuch
/contrib/mcp_sse_stateful_session/ @jue-yin @UNOWNED
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 保留了 1.36.4 版本的 `generic_proxy` owners，并添加了 `tap_sinks`
- Patch: 使用旧版本的 `generic_proxy` owners，并添加了 `mcp_sse_stateful_session`

**解决方案**: ✅ 合并两边
- 保留 HEAD 的 `generic_proxy` owners（更新的版本）
- 保留 HEAD 的 `tap_sinks` 条目
- 添加 patch 的 `mcp_sse_stateful_session` 条目

**最终结果**:
```
/contrib/generic_proxy/ @wbpcode @UNOWNED
/contrib/tap_sinks/ @coolg92003 @yiyibaoguo
/contrib/mcp_sse_stateful_session/ @jue-yin @UNOWNED
```

---

### 2. api/BUILD

**冲突位置**: 第83-87行，v3_protos deps

**冲突内容**:
```
<<<<<<< HEAD
=======
        "//contrib/envoy/extensions/filters/http/mcp_sse_stateful_session/v3alpha:pkg",
        "//contrib/envoy/extensions/filters/http/squash/v3:pkg",
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 无新增（目标分支不需要 squash）
- Patch: 添加 `mcp_sse_stateful_session` 和 `squash`

**解决方案**: ✅ 采用 patch 版本中的 `mcp_sse_stateful_session`，忽略 `squash`（1.36.4 不需要）

**最终结果**:
```
        "//contrib/envoy/extensions/filters/http/language/v3alpha:pkg",
        "//contrib/envoy/extensions/filters/http/mcp_sse_stateful_session/v3alpha:pkg",
        "//contrib/envoy/extensions/filters/http/sxg/v3alpha:pkg",
```

---

### 3. api/versioning/BUILD

**冲突位置**: 第21-25行，active_protos deps

**冲突内容**: 与 `api/BUILD` 相同

**解决方案**: ✅ 同上，只添加 `mcp_sse_stateful_session`

---

### 4. contrib/contrib_build_config.bzl

**冲突位置**: 两处冲突

**冲突1 (第18-22行)**:
```
<<<<<<< HEAD
=======
    "envoy.filters.http.mcp_sse_stateful_session":              "//contrib/mcp_sse_stateful_session/filters/http/source:config",
    "envoy.filters.http.squash":                                "//contrib/squash/filters/http/source:config",
>>>>>>> 012aa7596e
```

**冲突2 (第111-121行)**:
```
<<<<<<< HEAD
    # http tcp bridge plugin
    #

    "envoy.upstreams.http.tcp.golang":                          "//contrib/golang/upstreams/http/tcp/source:config",
=======
    # mcp sse stateful session
    #

    "envoy.http.mcp_sse_stateful_session.envelope":             "//contrib/mcp_sse_stateful_session/http/source:config",
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 有 `http tcp bridge plugin` 扩展
- Patch: 有 `mcp sse stateful session` 扩展

**解决方案**: ✅ 合并两边
- 冲突1: 只添加 `mcp_sse_stateful_session`（忽略 squash）
- 冲突2: 保留 HEAD 的 `http tcp bridge plugin`，再添加 `mcp sse stateful session`

---

### 5. contrib/extensions_metadata.yaml

**冲突位置**: 文件末尾

**冲突内容**:
```
<<<<<<< HEAD
envoy.upstreams.http.tcp.golang:
  categories:
  - envoy.upstreams
  security_posture: requires_trusted_downstream_and_upstream
  status: alpha
=======
envoy.filters.http.mcp_sse_stateful_session:
  ...
envoy.http.mcp_sse_stateful_session.envelope:
  ...
>>>>>>> 012aa7596e
```

**解决方案**: ✅ 合并两边，保留 HEAD 的 `upstreams.http.tcp.golang`，添加 patch 的两个 mcp 条目

---

### 6. envoy/http/filter.h

**冲突位置**: 第843-863行

**冲突内容**:
```cpp
<<<<<<< HEAD
  virtual void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost) PURE;
=======
  virtual void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost host) PURE;
>>>>>>> 012aa7596e

<<<<<<< HEAD
  virtual absl::optional<Upstream::LoadBalancerContext::OverrideHost>
  upstreamOverrideHost() const PURE;

  /**
   * @return true if the filter should shed load based on the system pressure, typically memory.
   */
  virtual bool shouldLoadShed() const PURE;
=======
  virtual absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const PURE;
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 有 `shouldLoadShed()` 方法（1.36.4 新增）
- Patch: 只是格式差异（参数命名 `host`）

**解决方案**: ✅ 采用 HEAD 版本
- 保留 HEAD 的所有接口（包括 `shouldLoadShed()`）
- 参数命名无影响

---

### 7. envoy/upstream/load_balancer.h

**冲突位置**: 第148-158行

**冲突内容**:
```cpp
<<<<<<< HEAD
  /**
   * Upstream override host. The first element is the target host address and the second element is
   * a boolean indicating whether the host should be selected strictly or not.
   * If the host should be selected strictly and no valid host is found, the load balancer should
   * return  nullptr.
   * If the host should not be selected strictly, the load balancer will select another host is the
   * target host is not valid.
   */
=======
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 有 `OverrideHost` 类型的详细注释说明
- Patch: 无此注释

**解决方案**: ✅ 采用 HEAD 版本（保留更完整的文档）

---

### 8. source/common/http/async_client_impl.h

**冲突位置**: 第253-261行

**冲突内容**:
```cpp
<<<<<<< HEAD
  absl::optional<Upstream::LoadBalancerContext::OverrideHost>
  upstreamOverrideHost() const override {
    return upstream_override_host_;
  }
  bool shouldLoadShed() const override { return false; }
=======
  absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const override { return {}; }
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 使用成员变量 `upstream_override_host_` 返回，并实现 `shouldLoadShed()`
- Patch: 简单返回空

**解决方案**: ✅ 采用 HEAD 版本（完整实现 + 新接口）

---

### 9. source/common/http/filter_manager.cc

**冲突位置**: 第2011-2036行

**冲突内容**:
```cpp
<<<<<<< HEAD
void ActiveStreamDecoderFilter::setUpstreamOverrideHost(
    Upstream::LoadBalancerContext::OverrideHost upstream_override_host) {
  parent_.upstream_override_host_.first.assign(upstream_override_host.first);
  parent_.upstream_override_host_.second = upstream_override_host.second;
}
// ... HEAD 版本存储 first 和 second 两个字段
=======
void ActiveStreamDecoderFilter::setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost host) {
  parent_.upstream_override_host_.emplace(std::move(host.first));
}
// ... Patch 版本只存储 first，second 硬编码为 false
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: `upstream_override_host_` 是 `std::pair<std::string, bool>` 类型，完整存储地址和 strict 标志
- Patch: `upstream_override_host_` 是 `absl::optional<std::string>` 类型，只存储地址

**解决方案**: ✅ 采用 HEAD 版本
- 理由：HEAD 版本支持完整的 strict 模式功能

---

### 10. source/common/http/filter_manager.h

**冲突位置**: 第308-316行

**冲突内容**:
```cpp
<<<<<<< HEAD
  void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost) override;
  absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const override;
  bool shouldLoadShed() const override;
  void sendGoAwayAndClose() override;
=======
  void setUpstreamOverrideHost(Upstream::LoadBalancerContext::OverrideHost host) override;
  absl::optional<Upstream::LoadBalancerContext::OverrideHost> upstreamOverrideHost() const override;
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 有 `shouldLoadShed()` 和 `sendGoAwayAndClose()` 方法声明
- Patch: 无这两个方法

**解决方案**: ✅ 采用 HEAD 版本（保留新增接口）

---

### 11. source/extensions/common/wasm/context.cc

**冲突位置**: 第1953-2001行

**冲突内容**:
```cpp
<<<<<<< HEAD
=======
std::string convertHealthStatusToString(Upstream::Host::Health status) {
  // ... Patch 添加了一个辅助函数和相关的 WASM 接口代码
}
// ... 约 50 行代码
#endif

>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 无此代码块
- Patch: 添加了 HIGRESS 条件编译下的 WASM 扩展函数：
  - `convertHealthStatusToString()` - 健康状态转字符串
  - `getUpstreamHosts()` - 获取上游主机列表
  - `setUpstreamOverrideHost()` - 设置上游覆盖主机

**验证结果**: 目标分支确实没有这些函数

**解决方案**: ✅ 采用 Patch 版本（保留新增的 WASM 接口函数）
- 这些是 mcp_sse_stateful_session 功能所需的 WASM 接口扩展
- 需要保留以支持新扩展的功能

**注意**: 需要修改 `setUpstreamOverrideHost` 中的 `false` 为 `effective_config_->isStrict()` 或保持 `false`（因为 WASM 调用可能没有 config 上下文）

---

### 12. source/extensions/filters/http/stateful_session/stateful_session.cc ⚠️

**冲突位置**: 第85-90行

**冲突内容**:
```cpp
<<<<<<< HEAD
    decoder_callbacks_->setUpstreamOverrideHost(
        std::make_pair(upstream_address.value(), effective_config_->isStrict()));
=======
    decoder_callbacks_->setUpstreamOverrideHost(std::make_pair(upstream_address.value(), false));
>>>>>>> 012aa7596e
```

**分析**:
- HEAD: 使用 `effective_config_->isStrict()` 动态决定是否严格模式
- Patch: 硬编码 `false`

**解决方案**: ✅ 采用 HEAD 版本
- 理由：1.36.4 版本支持配置 strict 模式，功能更完整
- patch 的逻辑（总是非严格模式）是 1.27.7 的简化版本

---

### 13. test/common/http/filter_manager_test.cc

**冲突位置**: 第423-435行

**冲突内容**:
```cpp
<<<<<<< HEAD
  decoder_filter->callbacks_->setUpstreamOverrideHost(std::make_pair("1.2.3.4", true));

  auto override_host = decoder_filter->callbacks_->upstreamOverrideHost();
  EXPECT_EQ(override_host.value().first, "1.2.3.4");
  EXPECT_TRUE(override_host.value().second);
=======
  decoder_filter->callbacks_->setUpstreamOverrideHost(std::make_pair("1.2.3.4", false));

  auto override_host = decoder_filter->callbacks_->upstreamOverrideHost();
  EXPECT_EQ(override_host.value().first, "1.2.3.4");
  EXPECT_EQ(override_host.value().second, false);
>>>>>>> 012aa7596e
```

**解决方案**: ✅ 采用 HEAD 版本（测试 strict=true 场景）

---

### 14. test/extensions/filters/http/stateful_session/stateful_session_test.cc

**冲突位置**: 3处相似冲突（第108-115, 154-161, 204-211行）

**冲突内容**: 测试断言中是否检查 `host.second` 的值

**分析**:
- HEAD: 不检查 `host.second`（因为会动态变化）
- Patch: 检查 `host.second == false`

**解决方案**: ✅ 采用 HEAD 版本（与 strict 模式配合）

---

### 15. test/mocks/http/mocks.cc

**冲突位置**: 第119-124行

**冲突内容**: 格式差异，无实质区别

**解决方案**: ✅ 采用 HEAD 版本

---

### 16. test/mocks/http/mocks.h

**冲突位置**: 第347-355行

**冲突内容**:
```cpp
<<<<<<< HEAD
  MOCK_METHOD(void, setUpstreamOverrideHost, (Upstream::LoadBalancerContext::OverrideHost));
  MOCK_METHOD(absl::optional<Upstream::LoadBalancerContext::OverrideHost>, upstreamOverrideHost, (),
              (const));
  MOCK_METHOD(bool, shouldLoadShed, (), (const));
=======
  MOCK_METHOD(void, setUpstreamOverrideHost, (Upstream::LoadBalancerContext::OverrideHost host));
  MOCK_METHOD(absl::optional<Upstream::LoadBalancerContext::OverrideHost>, upstreamOverrideHost, (), (const));
>>>>>>> 012aa7596e
```

**解决方案**: ✅ 采用 HEAD 版本（包含 `shouldLoadShed` mock）

---

### 17. tools/extensions/extensions_schema.yaml

**冲突位置**: 第151-157行

**冲突内容**:
```yaml
<<<<<<< HEAD
- envoy.tap_sinks.udp_sink
- envoy.tracers.opentelemetry.resource_detectors
- envoy.tracers.opentelemetry.samplers
=======
- envoy.http.mcp_sse_stateful_session
>>>>>>> 012aa7596e
```

**解决方案**: ✅ 合并两边，保留 HEAD 的三个条目，添加 patch 的条目

---

## 解决方案总结

| 策略 | 文件数 | 说明 |
|------|--------|------|
| ✅ 合并两边 | 5 | 配置文件：CODEOWNERS, api/BUILD, api/versioning/BUILD, contrib/contrib_build_config.bzl, contrib/extensions_metadata.yaml, tools/extensions/extensions_schema.yaml |
| ✅ 采用 HEAD | 11 | 接口/逻辑相关：保留 1.36.4 的新接口和功能 |

## 风险评估

| 风险项 | 评估 |
|--------|------|
| 接口兼容性 | 🟢 低风险 - patch 新增的扩展使用标准接口 |
| 功能完整性 | 🟢 低风险 - 保留 HEAD 的 strict 模式支持 |
| 测试覆盖 | 🟡 中风险 - 需要编译和单测验证 |

## 验证清单

- [ ] 所有冲突文件已正确解决
- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 新增的 mcp_sse_stateful_session 扩展文件完整

---

**请审核以上方案，确认无误后回复"确认执行"开始解决冲突。**
