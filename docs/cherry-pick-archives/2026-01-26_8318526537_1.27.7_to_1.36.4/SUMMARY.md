# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-01-26
- **原始提交**: 8f5f6bae5a (1.27.7)
- **最终提交**: 8318526537 (1.36.4)
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **Patch 名称**: 010-wasm-abi-support-injectencode-and-control-upstreamhosts.patch

## 冲突概览
- **冲突文件数**: 4
- **编译问题**: 是 (接口适配)
- **单测问题**: 是 (Mock 类缺失方法)

## 关键决策

1. **proxy_wasm_cpp_host 版本**: 采用 HEAD 版本 (a0f625e4b84949bfb9c19c5742a3331e32c7cc1b)
   - 原因: 用户确认 1.36.4 的版本已包含所需的新 ABI 功能

2. **health_checker_base_impl.cc/.h 成员变量**: 合并双方
   - HEAD: always_log_health_check_success_ (1.36.4 新特性)
   - Incoming: store_metrics_ (HIGRESS 条件编译)

3. **测试用例合并**: 保留 HEAD 的 Payload 测试 + 添加 HIGRESS LLM Service 测试
   - 接口适配: 移除 makeTestHost 的 simTime() 参数

4. **接口适配 setUpstreamOverrideHost**:
   - 1.36.4 使用 OverrideHost = std::pair<absl::string_view, bool>
   - 修改为 {address, true} 表示严格 host 选择

5. **Mock 类补充**: 在 MockHostLight 中添加 getEndpointMetrics/setEndpointMetrics 实现

## 修改的文件

### 源代码 (9 个文件)
- api/envoy/api/v2/core/health_check.proto
- api/envoy/config/core/v3/health_check.proto
- envoy/upstream/upstream.h
- source/common/upstream/upstream_impl.h
- source/extensions/common/wasm/context.cc
- source/extensions/common/wasm/context.h
- source/extensions/health_checkers/common/health_checker_base_impl.cc
- source/extensions/health_checkers/common/health_checker_base_impl.h
- source/extensions/health_checkers/http/health_checker_impl.cc

### 测试代码 (2 个文件)
- test/common/upstream/health_checker_impl_test.cc
- test/mocks/upstream/host.h (额外修改)

## 注意事项
- setUpstreamOverrideHost 接口在 1.36.4 中有变化，需要传递 std::pair<string_view, bool>
- 新增的 Host 接口方法需要在 Mock 类中实现

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md
- [ ] COMPILATION_FIX_PLAN.md (直接在代码中修复，未生成独立文档)
- [ ] TEST_FIX_PLAN.md (直接在代码中修复，未生成独立文档)
