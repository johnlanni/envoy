# 编译修复计划

## 编译检查结果摘要

| 问题类型 | 状态 | 说明 |
|---------|------|------|
| [问题1] | ⏳ 待修复 | [简要说明] |
| [问题2] | ⏳ 待修复 | [简要说明] |

**编译状态**: ❌ 失败 / ✅ 成功

---

## 修复的文件列表

| 文件 | 修改类型 |
|------|----------|
| `path/to/file1` | [修改类型] |
| `path/to/file2` | [修改类型] |

---

## 问题详情

### 1. [问题标题]

#### 问题描述

[描述编译错误信息]

```
[编译器错误输出]
```

#### 原因分析

[分析错误的根本原因]

#### 修复方案

[描述修复方案]

```cpp
// 原始代码
[错误代码]

// 修复后
[正确代码]
```

#### 状态

⏳ 待修复 / ✅ 已修复

---

### 2. [下一个问题]

...

---

## 常见编译错误类型参考

| 错误类型 | 典型错误信息 | 原因 | 修复方法 |
|---------|-------------|------|---------|
| Proto 重复字段 | `"field" is already defined` | Cherry-pick 引入重复定义 | 删除重复字段 |
| 接口签名不兼容 | `non-virtual member function marked 'override'` | 版本间接口变化 | 更新函数签名 |
| 抽象类实例化 | `variable type is an abstract class` | 缺少纯虚函数实现 | 实现所有纯虚函数 |
| 构造函数不匹配 | `no matching constructor` | 构造函数参数变化 | 添加正确的构造函数参数 |
| 指针访问语法 | `no member named 'x' in 'shared_ptr'` | shared_ptr 访问方式 | `.` 改为 `->` |
| 方法不存在 | `no member named 'methodName'` | 版本间 API 差异 | 添加或使用替代方法 |

---

## 待办事项

### 必须完成

- [ ] [修复项1]
- [ ] [修复项2]

### 可选排查

- [ ] [可选项1]

---

## 修复后的提交命令

```bash
# 查看修改
git diff HEAD

# 合并到 cherry-pick 提交
git add -A
git commit --amend --no-edit

# 或创建新的修复提交
git add -A
git commit -m "Fix: adapt code to target branch interface changes"
```
