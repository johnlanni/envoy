# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-01-23
- **原始提交**: 777c7eff6e (000-7-wasm-trace.patch), 4d48d4ef27 (000-8-wasm-custom.patch)
- **源分支**: 1.27.7
- **目标分支**: 1.36.4
- **操作人**: Automated Process

## 冲突概览
- 冲突文件数: 22 (from patch 000-8) + 5 (from patch 000-7) = 27 total
- 编译问题: 否
- 单测问题: 是 (resolved with additional fixes)

## 关键决策
1. **PropertyToken 枚举扩展**: 在 1.36.4 中添加了 HIGRESS 特定的 PropertyToken 值以支持新的追踪功能
2. **架构适配**: 将 HIGRESS 逻辑适配到 1.36.4 的新架构，如 `onHeadersModified()` 抽象方法
3. **PluginHandleSharedPtrThreadLocal 重构**: 适配 1.36.4 公开成员架构
4. **createFilter → createContext 迁移**: 适应 1.36.4 将过滤器创建方法移到基类的变化

## 注意事项
- HIGRESS 条件编译宏需要在所有相关代码中正确应用
- 接口签名从 `std::string_view` 适配到 `absl::string_view`
- proxy-wasm-cpp-host 版本需要更新以支持 `allow_on_headers_stop_iteration_`

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md
- [x] COMPILATION_FIX_PLAN.md (N/A - no compilation issues)
- [x] TEST_FIX_PLAN.md (N/A - test issues resolved in fixes)
- [x] commit_info.txt
- [x] modified_files.txt