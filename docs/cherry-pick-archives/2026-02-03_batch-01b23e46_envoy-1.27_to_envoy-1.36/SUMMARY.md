# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-02-03
- **原始提交** (批量):
  - `01b23e4618` - change lua filter function not found log to debug level
  - `2a9208486e` - envoy ai相关功能内存优化
  - `7ee22d3384` - fix(wasm): avoid clearing buffer when not modifying it
  - `cc9ebb23d1` - feat: add global request limit per I/O cycle with Wasm foreign function support
  - `c7195348d2` - Merge branch bugfix-modify-decoding-buffer into develop
  - `0961b00718` - remove useless func
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/batch-01b23e46-to-envoy-1.36
- **操作人**: Claude (AI Assistant)

## 冲突概览
- **冲突文件数**: 2
  - `source/common/http/conn_manager_impl.cc` (3个冲突位置)
  - `source/common/http/conn_manager_impl.h` (2个冲突位置)
- **编译问题**: 是 (ProtobufWkt → Protobuf namespace变更)
- **单测问题**: 是 (缺少 conn_manager_lib 依赖)
- **宏替换**: ALIMESH → HIGRESS

## 关键决策

### 决策 1: 冲突解决策略
**问题**: commit cc9ebb23d1 在 envoy-1.36 中产生架构差异冲突

**分析**:
- envoy-1.36 有 `direction_`, `allow_upstream_half_close_` 等新成员
- envoy-1.27 有 `refresh_rtt_after_request_` 成员
- 两个版本的构造函数结构不同

**决策**: 只移植全局限流功能，不移植 `refresh_rtt_after_request_` 特性
- 保留 envoy-1.36 的构造函数初始化列表不变
- 在构造函数体内添加全局限流初始化代码 (HIGRESS 宏包裹)
- 在 header 文件中只添加 thread_local 静态成员变量

### 决策 2: ProtobufWkt namespace 变更
**问题**: envoy-1.36 中 `ProtobufWkt` 已重命名为 `Protobuf`

**决策**: 将所有 `ProtobufWkt::Struct` 替换为 `Protobuf::Struct`
- 影响文件:
  - `source/common/http/filter_manager.cc` (line 499)
  - `source/extensions/filters/http/custom_response/custom_response_filter.cc` (lines 33, 74)

### 决策 3: ALIMESH → HIGRESS 宏替换
**问题**: 代码库中存在 ALIMESH 宏需要统一为 HIGRESS

**决策**: 全局搜索并替换所有 ALIMESH 宏
- 影响文件: 8个源文件和测试文件
- 确保条件编译宏统一

### 决策 4: 测试依赖修复
**问题**: wasm 相关测试链接失败，提示 `setGlobalMaxRequestsPerIoCycleForWasm` 未定义

**分析**: 这些函数在 `conn_manager_impl.cc` 中定义，但 wasm 测试的 BUILD 文件缺少依赖

**决策**: 在 `test/extensions/common/wasm/BUILD` 中添加依赖:
- `foreign_test` 添加 `//source/common/http:conn_manager_lib`
- `context_test` 添加 `//source/common/http:conn_manager_lib`
- `wasm_test` 添加 `//source/common/http:conn_manager_lib`

## 最终提交列表

1. `ab6a2323e6` - change lua filter function not found log to debug level
2. `6a067cced4` - envoy ai相关功能内存优化
3. `d43b411ed0` - fix(wasm): avoid clearing buffer when not modifying it
4. `85cb2be5fb` - feat: add global request limit per I/O cycle with Wasm foreign function support
5. `16275c9ba1` - Merge branch bugfix-modify-decoding-buffer into develop
6. `025bc2e721` - remove useless func
7. `43c441f9ac` - chore: replace ALIMESH macro with HIGRESS
8. `cdd7699dc9` - fix: replace ProtobufWkt with Protobuf for envoy-1.36 compatibility
9. `4acf702ebf` - fix: replace ProtobufWkt with Protobuf in custom_response_filter.cc
10. `f779ec36c7` - fix: add conn_manager_lib dependency to wasm tests

## 编译验证
✅ **编译成功**
- 编译目标:
  - `//source/common/http:conn_manager_lib`
  - `//source/extensions/common/wasm:wasm_lib`
  - `//source/extensions/filters/http/custom_response:config`

## 单元测试验证
✅ **测试通过**
- `//test/common/http:filter_manager_test` - PASSED
- `//test/extensions/common/wasm:foreign_test` - PASSED
- `//test/extensions/filters/http/custom_response:custom_response_filter_test` - PASSED

## 注意事项

### 架构差异
- envoy-1.36 不包含 `refresh_rtt_after_request_` 特性，只移植了全局限流功能
- 构造函数初始化列表保持 envoy-1.36 的原有结构

### 编译要点
- 使用 `Protobuf` namespace 而非 `ProtobufWkt`
- 所有条件编译使用 `HIGRESS` 宏

### 测试依赖
- wasm 相关测试必须包含 `conn_manager_lib` 依赖

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md (冲突解决文档)

## 技术要点

### Thread-local 全局限流实现
在 envoy-1.36 中成功移植了全局请求限流功能：
- 新增 thread_local 静态变量跟踪全局请求数
- 新增 Wasm foreign functions 用于动态控制限流
- 新增 gauge 指标 `downstream_rq_deferred` 监控延迟请求数
- 使用 libevent evwatch prepare 机制重置计数器

### 关键代码修改位置
1. **conn_manager_impl.h**: 新增 3 个 thread_local 静态成员
2. **conn_manager_impl.cc**: 新增全局限流逻辑和 Wasm 接口函数
3. **conn_manager_config.h**: 新增 `downstream_rq_deferred` gauge 统计
4. **foreign.cc**: 注册 Wasm foreign functions
5. **BUILD 文件**: 更新测试依赖关系

## 经验总结

### 成功要素
1. ✅ 仔细分析架构差异，选择性移植功能
2. ✅ 及时发现并修复 namespace 变更
3. ✅ 全局搜索确保宏替换完整
4. ✅ 正确识别链接错误的根因（缺少依赖）

### 遇到的挑战
1. ⚠️ envoy-1.27 和 envoy-1.36 架构差异较大
2. ⚠️ ProtobufWkt namespace 变更需要多处修复
3. ⚠️ 测试依赖关系需要仔细梳理

### 改进建议
1. 📋 对于跨大版本的 cherry-pick，建议先评估架构兼容性
2. 📋 namespace 变更类的问题可以考虑写脚本批量检查
3. 📋 BUILD 文件依赖关系可以通过静态分析工具提前发现

---

**归档日期**: 2026-02-03  
**归档版本**: v1.0
