# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `8f5f6bae5a` - Apply patch: 010-wasm-abi-support-injectencode-and-control-upstreamhosts.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 4 个

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `bazel/repository_locations.bzl` | 行 1523-1529 | 版本号冲突 |
| 2 | `source/extensions/health_checkers/common/health_checker_base_impl.cc` | 行 21-30 | 成员初始化冲突 |
| 3 | `source/extensions/health_checkers/common/health_checker_base_impl.h` | 行 107-113 | 成员变量声明冲突 |
| 4 | `test/common/upstream/health_checker_impl_test.cc` | 行 6875-6904, 6909-6934, 6941-7291 | 测试用例冲突 |

---

## 详细冲突分析与解决方案

### 1. `bazel/repository_locations.bzl`

#### 冲突点 1 (行 1523-1529)

**冲突描述**: proxy_wasm_cpp_host 依赖版本冲突

- **HEAD (1.36.4)**: 
  ```
  version = "a0f625e4b84949bfb9c19c5742a3331e32c7cc1b"
  sha256 = "6e36909f9c888aba56d71b17b07a0a14ab38f89c9fef5036da2d6e7a9ddc27e9"
  ```
- **Incoming (1.27.7 patch)**: 
  ```
  version = "27df3052072dd97527945c4167ef912020a0a92e"
  sha256 = "523ef02a84a69b03bdcde76053f3f412d73a20150f4bb11d6c11a127ed7df7dc"
  ```

**解决方案**: ✅ 采用 Incoming 版本

**理由**: Incoming 版本是 patch 引入的新功能所需的依赖版本，包含了 wasm ABI 支持 injectEncode 和 control upstream hosts 的新接口。

```python
    proxy_wasm_cpp_host = dict(
        project_name = "WebAssembly for Proxies (C++ host implementation)",
        project_desc = "WebAssembly for Proxies (C++ host implementation)",
        project_url = "https://github.com/higress-group/proxy-wasm-cpp-host",
        version = "27df3052072dd97527945c4167ef912020a0a92e",
        sha256 = "523ef02a84a69b03bdcde76053f3f412d73a20150f4bb11d6c11a127ed7df7dc",
```

---

### 2. `source/extensions/health_checkers/common/health_checker_base_impl.cc`

#### 冲突点 1 (行 21-30)

**冲突描述**: 构造函数初始化列表冲突 - 两个不同的成员变量初始化

- **HEAD (1.36.4)**: 
  ```cpp
  always_log_health_check_success_(config.always_log_health_check_success()), cluster_(cluster),
  dispatcher_(dispatcher), timeout_(PROTOBUF_GET_MS_REQUIRED(config, timeout)),
  ```
- **Incoming (1.27.7 patch)**: 
  ```cpp
  #if defined(HIGRESS)
      store_metrics_(config.store_metrics()),
  #endif
        cluster_(cluster), dispatcher_(dispatcher),
        timeout_(PROTOBUF_GET_MS_REQUIRED(config, timeout)),
  ```

**解决方案**: ⚠️ 合并双方

**理由**: 
- `always_log_health_check_success_` 是 1.36.4 的新特性，必须保留
- `store_metrics_` 是 HIGRESS 条件编译的特性，也必须保留
- 两者是独立的特性，互不冲突

```cpp
      always_log_health_check_success_(config.always_log_health_check_success()),
#if defined(HIGRESS)
      store_metrics_(config.store_metrics()),
#endif
      cluster_(cluster), dispatcher_(dispatcher),
      timeout_(PROTOBUF_GET_MS_REQUIRED(config, timeout)),
```

---

### 3. `source/extensions/health_checkers/common/health_checker_base_impl.h`

#### 冲突点 1 (行 107-113)

**冲突描述**: 成员变量声明冲突

- **HEAD (1.36.4)**: 
  ```cpp
  const bool always_log_health_check_success_;
  ```
- **Incoming (1.27.7 patch)**: 
  ```cpp
  #if defined(HIGRESS)
    const bool store_metrics_;
  #endif
  ```

**解决方案**: ⚠️ 合并双方

**理由**: 与 .cc 文件对应，需要同时保留两个成员变量声明

```cpp
  const bool always_log_health_check_failures_;
  const bool always_log_health_check_success_;
#if defined(HIGRESS)
  const bool store_metrics_;
#endif
  const Cluster& cluster_;
```

---

### 4. `test/common/upstream/health_checker_impl_test.cc`

#### 冲突点 1-3 (行 6875-7291)

**冲突描述**: 大量不同测试用例冲突

- **HEAD (1.36.4)**: 包含 HTTP health check payload 功能的测试用例：
  - `PayloadPostMethod`
  - `PayloadPutMethod`  
  - `PayloadGetMethodThrowsError`
  - `PayloadHeadMethodThrowsError`
  - `PayloadDeleteMethodThrowsError`
  - `PayloadTraceMethodThrowsError`
  - `PayloadOptionsMethodSuccess`
  - `PayloadPatchMethodSuccess`
  - `NoPayloadGetMethodDefault`
  - `MinimalPayloadPostMethod`

- **Incoming (1.27.7 patch)**: 包含 HIGRESS LLM Service health check 测试：
  - `LLMServiceHealthCheckSuccess` (在 `#if defined(HIGRESS)` 条件编译内)
  - `setupLLMServiceWithExpectedResponseHC()` helper 方法 (在 `#if defined(HIGRESS)` 条件编译内)

**解决方案**: ⚠️ 合并双方

**理由**: 
- HEAD 的 Payload 测试是 1.36.4 的新功能测试，必须保留
- Incoming 的 HIGRESS 测试是 patch 引入的功能测试，也必须保留
- 两者是独立的测试用例，需要仔细合并

**🔴 接口差异警告**:
- 1.27.7 使用: `makeTestHost(cluster_->info_, "tcp://127.0.0.1:80", simTime())`
- 1.36.4 使用: `makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")` (无 simTime 参数)
- **必须适配**: Incoming 的测试代码需要移除 `simTime()` 参数

**合并策略**:
1. 保留 HEAD 全部的 Payload 相关测试
2. 在文件末尾（namespace 闭合前）添加 HIGRESS 条件编译的测试
3. **修改 Incoming 测试代码**：移除 `makeTestHost` 调用中的 `simTime()` 参数

---

## 解决步骤总结

### ✅ 可直接合并的冲突

以下文件可以采用简单策略解决:
- `bazel/repository_locations.bzl` - 采用 Incoming 版本

### ⚠️ 需要手动合并

以下文件需要仔细手动处理:
1. `source/extensions/health_checkers/common/health_checker_base_impl.cc` - 合并两个成员初始化
2. `source/extensions/health_checkers/common/health_checker_base_impl.h` - 合并两个成员声明
3. `test/common/upstream/health_checker_impl_test.cc` - 合并两组独立的测试用例

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟢 低 | `bazel/repository_locations.bzl` | 简单的版本号替换 |
| 🟡 中 | `health_checker_base_impl.cc/.h` | 需要正确合并初始化顺序 |
| 🟡 中 | `health_checker_impl_test.cc` | 测试代码合并，需确保两组测试都能正常运行 |

---

## 编译修复

### 接口适配: `setUpstreamOverrideHost`

**问题**: 1.36.4 中 `setUpstreamOverrideHost` 接口签名已改变
- 1.27.7: `setUpstreamOverrideHost(std::string_view address)`
- 1.36.4: `setUpstreamOverrideHost(OverrideHost)` where `OverrideHost = std::pair<absl::string_view, bool>`

**修复**: 在 `source/extensions/common/wasm/context.cc` 中：
```cpp
// 修改前
decoder_callbacks_->setUpstreamOverrideHost(address);

// 修改后
decoder_callbacks_->setUpstreamOverrideHost({address, true});
```

---

## 测试修复

### Mock 类缺失方法: `getEndpointMetrics` / `setEndpointMetrics`

**问题**: patch 在 `envoy/upstream/upstream.h` 中添加了新的纯虚函数，但 mock 类未实现

**修复**: 在 `test/mocks/upstream/host.h` 的 `MockHostLight` 类中添加：
```cpp
#if defined(HIGRESS)
  std::string getEndpointMetrics() const override { return endpoint_metrics_; }
  void setEndpointMetrics(absl::string_view endpoint_metrics) override {
    endpoint_metrics_ = std::string(endpoint_metrics);
  }
#endif

// 成员变量
#if defined(HIGRESS)
  std::string endpoint_metrics_;
#endif
```

---

## 验证清单

- [x] 编译通过
- [x] 单元测试通过
- [ ] 功能验证通过
- [x] 条件编译正确（HIGRESS 宏）
- [x] proxy_wasm_cpp_host 版本正确（采用 HEAD 版本）

---

## 建议的工作流程

1. **第一阶段**: 解决 cherry-pick 冲突，完成基础合并
2. **第二阶段**: 编译验证 (`bazel build --config=clang`)
3. **第三阶段**: 运行相关单元测试验证
