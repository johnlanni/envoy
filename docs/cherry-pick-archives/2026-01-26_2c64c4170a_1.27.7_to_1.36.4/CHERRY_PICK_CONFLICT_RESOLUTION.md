# Cherry-pick 冲突解决方案

## 概述

| 项目 | 值 |
|------|------|
| **原始提交** | `2c64c4170a` |
| **提交信息** | Apply patch: 011-wasm-abi-support-injectencodeonheader.patch |
| **源分支** | 1.27.7 |
| **目标分支** | 1.36.4 |
| **冲突文件数** | 2 |

## Patch 功能描述

此 patch 为 WASM ABI 添加了 `injectEncodedDataToFilterChainOnHeader` 功能支持：

1. **新增方法**: `Context::injectEncodedDataToFilterChainOnHeader()` - 在 header 阶段注入编码数据到 filter chain
2. **新增 proto**: `inject_encoded_data.proto` - 定义 `InjectEncodedDataToFilterChainArguments` 消息
3. **新增 Foreign Function**: 
   - `inject_encoded_data_to_filter_chain` - 注入编码数据
   - `inject_encoded_data_to_filter_chain_on_header` - 在 header 阶段注入编码数据

## 冲突文件列表

| # | 文件路径 | 冲突块数 | 冲突类型 |
|---|----------|----------|----------|
| 1 | `source/extensions/common/wasm/ext/BUILD` | 2 | 新增内容位置冲突 |
| 2 | `source/extensions/common/wasm/foreign.cc` | 1 | include 语句冲突 |

---

## 冲突详细分析

### 冲突 1: source/extensions/common/wasm/ext/BUILD

#### 冲突位置
- 冲突块 1: 行 108-118
- 冲突块 2: 行 124-135

#### 冲突内容分析

**冲突块 1 (proto_library 定义)**:

| 版本 | 内容 |
|------|------|
| HEAD (1.36.4) | `set_envoy_filter_state_proto` - 依赖 `declare_property_proto` |
| Incoming (1.27.7 patch) | `inject_encoded_data_proto` - 依赖 `@com_google_protobuf//:struct_proto` |

**冲突块 2 (cc_proto_library 定义)**:

| 版本 | 内容 |
|------|------|
| HEAD (1.36.4) | `set_envoy_filter_state_cc_proto` |
| Incoming (1.27.7 patch) | `inject_encoded_data_cc_proto` |

#### 冲突原因

1.36.4 分支在 BUILD 文件末尾已有 `set_envoy_filter_state_*` proto 定义（这是 1.27.7 没有的功能）。
Patch 想要在同一位置添加 `inject_encoded_data_*` proto 定义。
两者实际上是**独立的功能**，都需要保留。

#### ✅ 解决方案

**保留两者** - 将 HEAD 的 `set_envoy_filter_state_*` 和 Incoming 的 `inject_encoded_data_*` 都保留。

最终内容应为：

```python
# NB: this target is compiled both to native code and to Wasm. Hence the generic rule.
proto_library(
    name = "set_envoy_filter_state_proto",
    srcs = ["set_envoy_filter_state.proto"],
    deps = [
        "declare_property_proto",
    ],
)

# NB: this target is compiled both to native code and to Wasm. Hence the generic rule.
cc_proto_library(
    name = "set_envoy_filter_state_cc_proto",
    deps = [":set_envoy_filter_state_proto"],
)

# NB: this target is compiled both to native code and to Wasm. Hence the generic rule.
proto_library(
    name = "inject_encoded_data_proto",
    srcs = ["inject_encoded_data.proto"],
    deps = [
        "@com_google_protobuf//:struct_proto",
    ],
)

# NB: this target is compiled both to native code and to Wasm. Hence the generic rule.
cc_proto_library(
    name = "inject_encoded_data_cc_proto",
    deps = [
        ":inject_encoded_data_proto",
        # "//external:protobuf_clib",
    ],
)
```

---

### 冲突 2: source/extensions/common/wasm/foreign.cc

#### 冲突位置
- 行 3-8

#### 冲突内容分析

| 版本 | include 语句 |
|------|--------------|
| HEAD (1.36.4) | `set_envoy_filter_state.pb.h`, `verify_signature.pb.h` |
| Incoming (1.27.7 patch) | `inject_encoded_data.pb.h` |

#### 冲突原因

1.36.4 已有 `set_envoy_filter_state.pb.h` 和 `verify_signature.pb.h`。
Patch 想要添加 `inject_encoded_data.pb.h`。
三者都需要。

#### ✅ 解决方案

**合并所有 include 语句**：

```cpp
#include "source/common/common/logger.h"
#include "source/extensions/common/wasm/ext/declare_property.pb.h"
#include "source/extensions/common/wasm/ext/inject_encoded_data.pb.h"
#include "source/extensions/common/wasm/ext/set_envoy_filter_state.pb.h"
#include "source/extensions/common/wasm/ext/verify_signature.pb.h"
#include "source/extensions/common/wasm/wasm.h"
```

---

## 风险评估

| 风险点 | 级别 | 说明 |
|--------|------|------|
| 功能冲突 | 低 | 两个功能独立，不存在冲突 |
| 接口兼容性 | 低 | 新增功能，不修改现有接口 |
| 编译问题 | 低 | 只是添加新的 proto 定义和 include |

## 验证清单

- [ ] BUILD 文件语法正确
- [ ] 所有 include 语句正确
- [ ] 编译通过
- [ ] 单元测试通过

## 解决方案分类

| 冲突 | 分类 | 说明 |
|------|------|------|
| BUILD 冲突 | ✅ 可直接合并 | 保留两个版本的 proto 定义 |
| foreign.cc include 冲突 | ✅ 可直接合并 | 合并所有 include 语句 |

---

## 执行步骤

1. 解决 `source/extensions/common/wasm/foreign.cc` 的 include 冲突
2. 解决 `source/extensions/common/wasm/ext/BUILD` 的两个冲突块
3. 标记文件已解决并完成 cherry-pick
4. 编译验证
5. 运行相关单元测试
