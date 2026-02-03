# Cherry-pick 归档总结

## 基本信息
- **日期**: 2026-02-03
- **原始提交**: 8dfc0df4e6c18cbaed4a504b4f225f959ef03d2b
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/8dfc0df4-to-envoy-1.36
- **操作人**: zhangty

## 提交信息
**标题**: fix: prevent WASM VM accumulation during rebuild by tracking old handle lifetime

**描述**: 在 `wasm.cc` 和 `wasm.h` 文件中添加了对旧 Wasm VM 句柄的弱引用检查，以防止在旧句柄仍在使用时创建新的 VM 实例，从而避免内存累积问题。

**核心功能**:
- 添加 `weak_ptr<PluginHandle> old_handle_` 成员追踪先前的 VM 实例
- 在允许 rebuild 之前检查旧句柄是否仍然存活
- 只在先前的 VM 完全释放后才允许 rebuild
- 在替换之前将当前句柄存储为 weak_ptr

这确保了同一时间最多只存在一个旧 VM 实例，在保持现有基于时间的节流机制的同时防止无限制的内存增长。

## 冲突概览
- **冲突文件数**: 2
  - `source/extensions/common/wasm/wasm.cc`
  - `source/extensions/common/wasm/wasm.h`
- **编译问题**: 否
- **单测问题**: 否

## 架构差异
envoy-1.27 和 envoy-1.36 在 `PluginHandleSharedPtrThreadLocal` 类的架构设计上存在显著差异：

### envoy-1.27 架构（源分支）
```cpp
class PluginHandleSharedPtrThreadLocal {
  PluginHandleSharedPtr handle_;  // 私有成员
  PluginHandleSharedPtr& handle() { return handle_; }  // getter 访问
};
```

### envoy-1.36 架构（目标分支）
```cpp
class PluginHandleSharedPtrThreadLocal {
  PluginHandleSharedPtr handle{};  // 公有成员，直接访问
  MonotonicTime last_load{};  // 新增成员
};
```

## 关键决策

### 1. 架构适配策略
**决策**: 保留 envoy-1.36 的架构，将 patch 功能适配到目标分支
**原因**: 
- 目标分支已经重构为更简洁的公有成员模式
- 保持目标分支架构一致性优于回退到旧架构
- 只需进行变量命名适配即可移植功能

### 2. 变量命名转换
**决策**: 所有 `handle_` 引用改为 `handle`
**原因**: 
- envoy-1.36 使用公有成员 `handle` 而非私有成员 `handle_`
- 保持代码风格与目标分支一致

### 3. 新增成员变量位置
**决策**: 在 `PluginHandleSharedPtrThreadLocal` 类的 private 区域添加 `std::weak_ptr<PluginHandle> old_handle_`
**原因**: 
- 遵循 patch 的原始设计
- 在 HIGRESS 条件编译区域内添加，保持条件编译一致性

### 4. fail_recovery 场景处理
**决策**: 添加 `!is_fail_recovery` 条件到 old_handle 检查
**原因**: 
- 源分支（envoy-1.27）的完整实现包含此条件
- Cherry-pick 过程中注释被正确保留，但条件判断需要手动添加
- 确保 fail recovery 场景可以跳过 old_handle 检查，保证恢复能够进行

## 解决方案实施

### wasm.h 修改
- 保留 envoy-1.36 的类结构（公有成员 `handle`、`last_load` 等）
- 在 HIGRESS 条件编译的 private 区域添加 `std::weak_ptr<PluginHandle> old_handle_`

### wasm.cc 修改
- 在时间间隔检查后添加 old_handle 的 expired 检查：
  ```cpp
  if (!is_fail_recovery && !old_handle_.expired()) {
    ENVOY_LOG(info, "old wasm vm handle is still in use, skipping rebuild to prevent VM accumulation");
    return false;
  }
  ```
- 在更新 handle 之前添加：
  ```cpp
  old_handle_ = handle;
  ```
- 所有 `handle_` 引用改为 `handle`

## 验证结果

### 编译验证 ✅
- **目标**: `//source/extensions/common/wasm:wasm_lib`
- **结果**: 编译成功，无错误

### 单元测试验证 ✅
- **目标**: 
  - `//test/extensions/common/wasm:wasm_test`
  - `//test/extensions/common/wasm:plugin_test`
- **结果**: 所有测试通过

## 风险评估
**整体风险**: 🟢 低

### 已缓解的风险
- ✅ 变量命名适配：所有 `handle_` 已正确改为 `handle`
- ✅ 新增成员变量：`old_handle_` 已添加到正确位置
- ✅ 逻辑移植：expired 检查和 handle 保存逻辑已正确实现
- ✅ fail_recovery 条件：已添加条件判断确保恢复场景正常工作
- ✅ 编译验证：编译通过
- ✅ 单测验证：所有相关测试通过

## 注意事项

### 关键点
1. **架构差异**: envoy-1.27 使用 `handle_` 私有成员，envoy-1.36 使用 `handle` 公有成员
2. **条件编译**: 新增的 `old_handle_` 成员在 `#if defined(HIGRESS)` 块内
3. **fail_recovery 语义**: 当 `is_fail_recovery=true` 时，跳过 old_handle 检查，允许强制恢复

### 未来维护建议
- 如果需要修改 `PluginHandleSharedPtrThreadLocal` 类，注意 envoy-1.27 和 envoy-1.36 的架构差异
- 涉及 `handle` 成员的代码，在跨版本移植时需要检查是使用 `handle` 还是 `handle_`

## 相关文件
- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md - 冲突解决方案文档
- [x] commit_info.txt - 原始提交信息
- [x] modified_files.txt - 修改文件列表

## 增量更新记录

_无增量更新_

---

**归档完成时间**: 2026-02-03
**归档状态**: ✅ 完成
