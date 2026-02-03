# Cherry-pick 冲突解决方案

## 概述

- **Commit**: `8dfc0df4e6c18cbaed4a504b4f225f959ef03d2b`
- **标题**: fix: prevent WASM VM accumulation during rebuild by tracking old handle lifetime
- **源分支**: envoy-1.27
- **目标分支**: envoy-1.36
- **工作分支**: cherry-pick/8dfc0df4-to-envoy-1.36
- **冲突文件数**: 2

## 冲突文件列表

| 文件路径 | 冲突位置 | 冲突类型 | 风险等级 |
|---------|---------|---------|---------|
| `source/extensions/common/wasm/wasm.cc` | 行 227-244 | 变量命名差异 | 🟡 中等 |
| `source/extensions/common/wasm/wasm.h` | 行 189-206 | 类架构差异 | 🟡 中等 |

## 架构差异分析

### 关键架构变化

envoy-1.27 和 envoy-1.36 在 `PluginHandleSharedPtrThreadLocal` 类的设计上存在显著差异：

**envoy-1.27 架构（源分支）**:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr handle) : handle_(handle){};
  bool rebuild(bool is_fail_recovery = false);
  PluginHandleSharedPtr& handle() { return handle_; }

private:
  PluginHandleSharedPtr handle_;  // 私有成员，通过 handle() getter 访问
  MonotonicTime last_recover_time_;
  std::weak_ptr<PluginHandle> old_handle_;  // ← patch 新增
};
```

**envoy-1.36 架构（目标分支）**:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtr handle{};  // 公有成员，直接访问
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;

  bool rebuild(bool is_fail_recovery = false);

private:
  MonotonicTime last_recover_time_;
  // 缺少 old_handle_ ← 需要添加
};
```

**关键差异**:
1. **成员变量访问方式**: envoy-1.27 使用 `handle_` (私有) + `handle()` getter，envoy-1.36 使用 `handle` (公有)
2. **构造函数签名**: envoy-1.36 有额外的 `last_load` 参数
3. **成员变量名**: `handle_` vs `handle`

## 冲突详细分析

### 冲突 1: wasm.h - 类定义

**冲突位置**: `source/extensions/common/wasm/wasm.h:189-206`

**HEAD (envoy-1.36)** 版本:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtr handle{};
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;

  bool rebuild(bool is_fail_recovery = false);

private:
  MonotonicTime last_recover_time_;
};
```

**Incoming (patch)** 版本:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr handle) : handle_(handle){};
  bool rebuild(bool is_fail_recovery = false);
  PluginHandleSharedPtr& handle() { return handle_; }

private:
  PluginHandleSharedPtr handle_;
  MonotonicTime last_recover_time_;
  std::weak_ptr<PluginHandle> old_handle_;  // ← patch 新增
};
```

**分析**:
- Patch 的核心功能是添加 `old_handle_` 成员来追踪旧的 VM handle
- 目标分支架构已改为公有成员 `handle`，不再使用 `handle_` 和 `handle()` getter
- **解决方案**: 保留 envoy-1.36 的架构，只添加 `old_handle_` 成员

**解决方案**:
```cpp
class PluginHandleSharedPtrThreadLocal : public ThreadLocal::ThreadLocalObject,
                                         public Logger::Loggable<Logger::Id::wasm> {
public:
  PluginHandleSharedPtr handle{};
  MonotonicTime last_load{};

  PluginHandleSharedPtrThreadLocal(PluginHandleSharedPtr h, MonotonicTime t = {})
      : handle(std::move(h)), last_load(t) {}
  PluginHandleSharedPtrThreadLocal() = default;

  bool rebuild(bool is_fail_recovery = false);

private:
  MonotonicTime last_recover_time_;
  std::weak_ptr<PluginHandle> old_handle_;  // ← 添加此行
};
```

**风险**: 🟡 中等 - 需要确保成员变量命名一致

---

### 冲突 2: wasm.cc - rebuild 函数实现

**冲突位置**: `source/extensions/common/wasm/wasm.cc:227-244`

**HEAD (envoy-1.36)** 版本:
```cpp
bool PluginHandleSharedPtrThreadLocal::rebuild(bool is_fail_recovery) {
  if (handle == nullptr || handle->wasmHandle() == nullptr ||
      handle->wasmHandle()->wasm() == nullptr) {
    ENVOY_LOG(warn, "wasm has not been initialized");
    return false;
  }
  auto& dispatcher = handle->wasmHandle()->wasm()->dispatcher();
  auto now = dispatcher.timeSource().monotonicTime() + cache_time_offset_for_testing;
  if (now - last_recover_time_ < std::chrono::seconds(MIN_RECOVER_INTERVAL_SECONDS)) {
    ENVOY_LOG(info, "rebuild interval has not been reached");
    return false;
  }
  // Even if rebuild fails, it will be retried after the interval
  last_recover_time_ = now;
  std::shared_ptr<PluginHandleBase> new_handle;
  if (handle->rebuild(new_handle)) {
    handle = std::static_pointer_cast<PluginHandle>(new_handle);  // ← 直接访问
    // Increment appropriate metrics based on rebuild type
    if (is_fail_recovery) {
      handle->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
      ENVOY_LOG(info, "wasm vm recover from crash success");
    } else {
      handle->wasmHandle()->wasm()->lifecycleStats().rebuild_total_.inc();
      ENVOY_LOG(info, "wasm vm rebuild success");
    }
    return true;
  }
  return false;
}
```

**Incoming (patch)** 版本:
```cpp
bool PluginHandleSharedPtrThreadLocal::rebuild(bool is_fail_recovery) {
  if (handle_ == nullptr || handle_->wasmHandle() == nullptr ||
      handle_->wasmHandle()->wasm() == nullptr) {
    ENVOY_LOG(warn, "wasm has not been initialized");
    return false;
  }
  auto& dispatcher = handle_->wasmHandle()->wasm()->dispatcher();
  auto now = dispatcher.timeSource().monotonicTime() + cache_time_offset_for_testing;
  if (now - last_recover_time_ < std::chrono::seconds(MIN_RECOVER_INTERVAL_SECONDS)) {
    ENVOY_LOG(info, "rebuild interval has not been reached");
    return false;
  }
  // Check if old handle is still alive (still being referenced by old requests)
  // If it's still alive, we don't want to create another VM to prevent memory accumulation
  // For fail recovery scenarios, skip this check to ensure recovery can proceed
  if (!is_fail_recovery && !old_handle_.expired()) {  // ← patch 新增的检查
    ENVOY_LOG(info, "old wasm vm handle is still in use, skipping rebuild to prevent VM accumulation");
    return false;
  }
  // Even if rebuild fails, it will be retried after the interval
  last_recover_time_ = now;
  std::shared_ptr<PluginHandleBase> new_handle;
  if (handle_->rebuild(new_handle)) {
    // Store weak_ptr to current handle before replacing it
    old_handle_ = handle_;  // ← patch 新增
    handle_ = std::static_pointer_cast<PluginHandle>(new_handle);  // ← 使用 handle_
    // Increment appropriate metrics based on rebuild type
    if (is_fail_recovery) {
      handle_->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
      ENVOY_LOG(info, "wasm vm recover from crash success");
    } else {
      handle_->wasmHandle()->wasm()->lifecycleStats().rebuild_total_.inc();
      ENVOY_LOG(info, "wasm vm rebuild success");
    }
    return true;
  }
  return false;
}
```

**分析**:
- Patch 新增的核心逻辑：
  1. 检查 `old_handle_` 是否仍然存活（`!old_handle_.expired()`）
  2. 在替换 handle 之前保存当前 handle 到 `old_handle_`
- 变量命名需要适配：所有 `handle_` 改为 `handle`（envoy-1.36 使用公有成员）

**解决方案**:
```cpp
bool PluginHandleSharedPtrThreadLocal::rebuild(bool is_fail_recovery) {
  if (handle == nullptr || handle->wasmHandle() == nullptr ||
      handle->wasmHandle()->wasm() == nullptr) {
    ENVOY_LOG(warn, "wasm has not been initialized");
    return false;
  }
  auto& dispatcher = handle->wasmHandle()->wasm()->dispatcher();
  auto now = dispatcher.timeSource().monotonicTime() + cache_time_offset_for_testing;
  if (now - last_recover_time_ < std::chrono::seconds(MIN_RECOVER_INTERVAL_SECONDS)) {
    ENVOY_LOG(info, "rebuild interval has not been reached");
    return false;
  }
  // Check if old handle is still alive (still being referenced by old requests)
  // If it's still alive, we don't want to create another VM to prevent memory accumulation
  // For fail recovery scenarios, skip this check to ensure recovery can proceed
  if (!is_fail_recovery && !old_handle_.expired()) {  // ← 添加此检查
    ENVOY_LOG(info, "old wasm vm handle is still in use, skipping rebuild to prevent VM accumulation");
    return false;
  }
  // Even if rebuild fails, it will be retried after the interval
  last_recover_time_ = now;
  std::shared_ptr<PluginHandleBase> new_handle;
  if (handle->rebuild(new_handle)) {
    // Store weak_ptr to current handle before replacing it
    old_handle_ = handle;  // ← 添加此行
    handle = std::static_pointer_cast<PluginHandle>(new_handle);
    // Increment appropriate metrics based on rebuild type
    if (is_fail_recovery) {
      handle->wasmHandle()->wasm()->lifecycleStats().recover_total_.inc();
      ENVOY_LOG(info, "wasm vm recover from crash success");
    } else {
      handle->wasmHandle()->wasm()->lifecycleStats().rebuild_total_.inc();
      ENVOY_LOG(info, "wasm vm rebuild success");
    }
    return true;
  }
  return false;
}
```

**风险**: 🟡 中等 - 需要确保所有变量引用正确

## 风险评估

### 整体风险: 🟡 中等

**风险点**:
1. ✅ **成员变量命名差异**: 所有 `handle_` 需改为 `handle`
2. ✅ **新增成员变量**: 在 private 区域添加 `old_handle_`
3. ✅ **逻辑移植**: 两处新增逻辑（expired 检查 + handle 保存）

**缓解措施**:
- 严格遵循目标分支的命名规范
- 保留 patch 的核心逻辑不变
- 编译验证确保接口一致

## 验证清单

### 代码正确性
- [ ] 冲突已按架构差异正确解决
- [ ] 所有 `handle_` 已改为 `handle`
- [ ] `old_handle_` 已添加到类的 private 区域
- [ ] 新增的检查逻辑已正确移植

### 编译验证
- [ ] 编译通过：`bazel build --config=clang //source/extensions/common/wasm:wasm_lib`
- [ ] 无新的编译错误或警告

### 单元测试
- [ ] 运行相关测试：`bazel test --config=clang //test/extensions/common/wasm:wasm_test`
- [ ] 所有测试通过

### 功能验证
- [ ] WASM VM rebuild 逻辑正常
- [ ] 旧 handle 引用检查生效
- [ ] 防止 VM 累积的功能正常

## 实施步骤

1. **解决 wasm.h 冲突**:
   - 保留 envoy-1.36 的类架构（公有成员 `handle`、`last_load` 等）
   - 在 private 区域添加 `std::weak_ptr<PluginHandle> old_handle_;`

2. **解决 wasm.cc 冲突**:
   - 在时间检查后添加 old_handle 的 expired 检查
   - 在更新 handle 之前添加 `old_handle_ = handle;`
   - 确保所有变量引用使用 `handle` 而非 `handle_`

3. **标记冲突已解决**:
   ```bash
   git add source/extensions/common/wasm/wasm.h
   git add source/extensions/common/wasm/wasm.cc
   ```

4. **完成 cherry-pick**:
   ```bash
   git cherry-pick --continue
   ```

5. **编译验证**（Phase 7）
6. **单元测试验证**（Phase 8）
7. **文档归档**（Phase 9）

---

## 总结

本次 cherry-pick 的核心挑战是适配 envoy-1.27 到 envoy-1.36 的架构变化（`handle_` 私有成员变为 `handle` 公有成员）。解决方案是：

1. **保留目标分支架构**: 使用公有成员 `handle` 和现有构造函数签名
2. **移植核心功能**: 添加 `old_handle_` 成员变量和相关检查逻辑
3. **适配变量命名**: 所有 `handle_` 引用改为 `handle`

这样既保持了目标分支的架构一致性，又完整移植了 patch 的 VM 累积防护功能。

---

**⚠️ 请审核以上方案，确认无误后回复"确认执行"开始解决冲突。**
