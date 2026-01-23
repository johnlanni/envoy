# 测试问题修复方案文档

## 测试结果概览

| 测试套件 | 失败测试数 | 通过测试数 | 状态 |
|---------|-----------|-----------|------|
| `<test_target_1>` | X | Y | ❌ FAILED |
| `<test_target_2>` | 0 | 全部 | ✅ PASSED |

---

## 失败的测试用例详情

### 1. `<TestSuiteName>.<TestCaseName>`

**位置**: `<file>:<line>`

**失败原因**:
```
<error message>
```

**失败类型**: [ ] Mock 未调用 / [ ] 断言失败 / [ ] 构造参数错误 / [ ] 其他

---

## 根因分析

### 问题根源

描述失败的根本原因，例如：
- 条件编译下使用不同的方法签名
- 接口变化导致返回值不同
- Mock 设置与实际调用不匹配

### 相关代码

**源代码** (`source/...`):
```cpp
// 在条件编译下调用不同签名的方法
#if defined(HIGRESS)
  result = obj->methodName(arg1, arg2, arg3);  // 三参数版本
#else
  result = obj->methodName(arg1);              // 单参数版本
#endif
```

**测试代码** (`test/...`):
```cpp
// 只 mock 了单参数版本
EXPECT_CALL(*mock, methodName(_)).Times(N);  // 在 HIGRESS 模式下不会被调用
```

---

## 修复方案

### 方案描述

为受影响的测试添加条件编译分支，确保在不同模式下使用正确的 mock 签名。

### 修改示例

```cpp
// 修复后
#if defined(HIGRESS)
  EXPECT_CALL(*mock, methodName(_, _, _))
      .Times(N)
      .WillRepeatedly(Return(expected_value));
#else
  EXPECT_CALL(*mock, methodName(_))
      .Times(N)
      .WillRepeatedly(Return(expected_value));
#endif
```

---

## 详细修改清单

### 文件 1: `test/xxx/xxx_test.cc`

| 行号 | 测试用例 | 修改内容 |
|-----|---------|---------|
| XXX | `TestCase1` | 添加条件编译分支 |
| YYY | `TestCase2` | 更新 mock 签名 |

---

## 修复步骤

1. **定位受影响的测试**: 搜索所有使用相关 mock 的测试

2. **应用条件编译**: 为每个受影响的 `EXPECT_CALL` 添加条件分支

3. **确保返回值一致**: HIGRESS 版本和非 HIGRESS 版本应返回相同的结果

4. **重新编译并测试**:
   ```bash
   bazel test --config=clang -c opt <test_target>
   ```

---

## 风险评估

| 风险项 | 级别 | 说明 |
|-------|------|------|
| 测试覆盖遗漏 | 低/中/高 | 描述 |
| Mock 行为不一致 | 低/中/高 | 描述 |

---

## 待确认事项

1. 是否还有其他测试需要同样修改？
2. 修复优先级确认

---

请确认修复方案后开始执行修复操作。
