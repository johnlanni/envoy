# Cherry-pick 冲突解决方案文档

## 概述

**提交信息**: `fd1950ce5b` - Apply patch: 013-fix-srds-direct-local-address.patch
**源分支**: 1.27.7
**目标分支**: 1.36.4
**冲突文件数**: 5 个

## 冲突文件列表

| 序号 | 文件路径 | 冲突位置 | 冲突类型 |
|------|----------|----------|----------|
| 1 | `source/common/http/conn_manager_impl.cc` | 行 1807-1825 | 变量名差异 + 新增逻辑 |
| 2 | `source/extensions/filters/http/lua/wrappers.h` | 行 294-298 | 目标分支新增功能 |
| 3 | `test/common/formatter/substitution_formatter_test.cc` | 行 908-930 | 重复代码冲突 |
| 4 | `test/common/router/header_formatter_test.cc` | 行 77-92 | API 语法差异 |
| 5 | `test/extensions/filters/http/lua/lua_integration_test.cc` | 行 638-658 | 测试参数化差异 |

---

## 详细冲突分析与解决方案

### 1. `source/common/http/conn_manager_impl.cc`

#### 冲突点 1 (行 1807-1825)

**冲突描述**: patch 添加了 SRDS 重试查找的统计计数逻辑，但 1.36.4 使用了不同的变量名

- **HEAD (1.36.4)**: 
  - 使用 `route_result` 变量存储路由查找结果
  - 日志输出使用 `route_result.route != nullptr`

- **Incoming (1.27.7 patch)**: 
  - 使用 `route` 变量存储路由查找结果
  - 添加了统计计数逻辑 (`downstream_rq_retry_scope_found_total_` / `downstream_rq_retry_scope_not_found_total_`)

**解决方案**: ⚠️ 需要手动合并 - 保留 1.36.4 的变量名，同时添加 patch 的统计计数逻辑

```cpp
      route_result = snapped_route_config_->route(cb, *request_headers_, filter_manager_.streamInfo(),
                                                  stream_id_);
      bool retry_found = route_result.route != nullptr;
      ENVOY_STREAM_LOG(debug,
                       "after the route was not found, search again in other scopes and found:{}",
                       *this, retry_found);
      if (retry_found) {
        connection_manager_.stats_.named_.downstream_rq_retry_scope_found_total_.inc();
      } else {
        connection_manager_.stats_.named_.downstream_rq_retry_scope_not_found_total_.inc();
      }
```

---

### 2. `source/extensions/filters/http/lua/wrappers.h`

#### 冲突点 1 (行 294-298)

**冲突描述**: 1.36.4 新增了 `dynamicTypedMetadata` 和 `filterState` 两个 Lua wrapper 函数

- **HEAD (1.36.4)**: 
  ```cpp
  {"dynamicTypedMetadata", static_luaDynamicTypedMetadata},
  {"filterState", static_luaFilterState},
  ```

- **Incoming (1.27.7 patch)**: 
  - (空) - 源分支没有这两个函数

**解决方案**: ✅ 采用 HEAD 版本 - 保留 1.36.4 新增的功能

```cpp
            {"dynamicTypedMetadata", static_luaDynamicTypedMetadata},
            {"filterState", static_luaFilterState},
            {"downstreamDirectLocalAddress", static_luaDownstreamDirectLocalAddress},
```

---

### 3. `test/common/formatter/substitution_formatter_test.cc`

#### 冲突点 1 (行 908-930)

**冲突描述**: HEAD 版本中存在与冲突后代码重复的测试用例

- **HEAD (1.36.4)**: 
  - 包含多个测试块：`DOWNSTREAM_DIRECT_LOCAL_ADDRESS_WITHOUT_PORT` 测试、`DOWNSTREAM_DIRECT_LOCAL_PORT` 测试、以及开始的 `DOWNSTREAM_LOCAL_PORT` 声明

- **Incoming (1.27.7 patch)**: 
  - (空)

**分析**: 查看冲突后的代码（行 932-944），发现已经存在相同的测试代码，这意味着 HEAD 版本的内容是重复的。

**解决方案**: ✅ 采用 Incoming 版本 - 删除重复代码，保留冲突后的测试代码

---

### 4. `test/common/router/header_formatter_test.cc`

#### 冲突点 1 (行 77-92)

**冲突描述**: `UPSTREAM_METADATA` 格式语法在两个版本间存在差异

- **HEAD (1.36.4)**: 
  ```cpp
  {"%UPSTREAM_METADATA(ns:key)%", {"value"}, {}},
  {"[%UPSTREAM_METADATA(ns:key)%", {"[value"}, {}},
  {"%UPSTREAM_METADATA(ns:key)%]", {"value]"}, {}},
  {"[%UPSTREAM_METADATA(ns:key)%]", {"[value]"}, {}},
  {"%UPSTREAM_METADATA(ns:key)%", {"value"}, {}},
  ```
  使用简化语法 `(ns:key)`

- **Incoming (1.27.7 patch)**: 
  ```cpp
  {"%UPSTREAM_METADATA([\"ns\", \"key\"])%", {"value"}, {}},
  ...
  ```
  使用 JSON 数组语法 `(["ns", "key"])`

**解决方案**: ✅ 采用 HEAD 版本 - 这是 1.36.4 的架构特性，应保持目标分支的 API 风格

---

### 5. `test/extensions/filters/http/lua/lua_integration_test.cc`

#### 冲突点 1 (行 638-658)

**冲突描述**: 测试参数化方式在两个版本间存在差异

- **HEAD (1.36.4)**: 
  ```cpp
  std::get<0>(GetParam()).version == Network::Address::IpVersion::v4
  ```
  使用 `std::get<0>(GetParam()).version` 访问 IP 版本

- **Incoming (1.27.7 patch)**: 
  ```cpp
  GetParam() == Network::Address::IpVersion::v4
  ```
  直接使用 `GetParam()` 作为 IP 版本

**解决方案**: ✅ 采用 HEAD 版本 - 1.36.4 的测试参数化结构不同，应保持目标分支的测试框架

---

## 🔴 架构差异警告

### 架构对比

| 组件 | 源分支 (1.27.7) | 目标分支 (1.36.4) |
|------|-----------------|-------------------|
| route 变量类型 | `route` (单一指针) | `route_result` (可能是结构体) |
| UPSTREAM_METADATA 语法 | JSON 数组 `(["ns", "key"])` | 简化语法 `(ns:key)` |
| 测试参数化 | 直接 `GetParam()` | `std::get<0>(GetParam()).version` |
| Lua wrapper 函数 | 无 dynamicTypedMetadata/filterState | 有 |

### 影响

- `conn_manager_impl.cc`: 需要适配变量名，同时保留 patch 的统计逻辑
- 测试文件: 需要保持 1.36.4 的架构特性，不能直接采用 patch 的测试代码

---

## 解决步骤总结

### ✅ 可直接合并的冲突

以下文件可以采用简单策略解决:
- `source/extensions/filters/http/lua/wrappers.h` - 采用 HEAD 版本
- `test/common/formatter/substitution_formatter_test.cc` - 采用 Incoming 版本（删除重复）
- `test/common/router/header_formatter_test.cc` - 采用 HEAD 版本
- `test/extensions/filters/http/lua/lua_integration_test.cc` - 采用 HEAD 版本

### ⚠️ 需要手动合并

以下文件需要仔细手动处理:
1. `source/common/http/conn_manager_impl.cc` - 需要合并双方：保留 1.36.4 变量名 + 添加 patch 统计逻辑

### 🔧 需要额外适配的工作

无额外适配工作需要。

---

## 风险评估

| 风险等级 | 文件 | 说明 |
|----------|------|------|
| 🟡 中 | `conn_manager_impl.cc` | 需要手动合并，需验证统计逻辑正确性 |
| 🟢 低 | `wrappers.h` | 简单保留 HEAD 版本 |
| 🟢 低 | `substitution_formatter_test.cc` | 删除重复代码 |
| 🟢 低 | `header_formatter_test.cc` | 保留 HEAD 版本 |
| 🟢 低 | `lua_integration_test.cc` | 保留 HEAD 版本 |

---

## 验证清单

- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 功能验证通过
- [ ] 条件编译正确（如 HIGRESS 等）
- [ ] 额外适配工作完成

---

## 建议的工作流程

1. **第一阶段**: 解决 cherry-pick 冲突，完成基础合并
2. **第二阶段**: 编译验证
3. **第三阶段**: 运行单元测试验证
