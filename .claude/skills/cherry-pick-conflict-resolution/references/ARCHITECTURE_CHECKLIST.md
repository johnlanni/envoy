# 架构差异检查清单

## 检查表

在深度验证阶段，使用此清单对比源分支和目标分支的架构差异。

### 1. 类/结构体位置

| 检查项 | 源分支 | 目标分支 | 是否一致 |
|--------|--------|----------|----------|
| 头文件位置 | | | ☐ |
| 实现文件位置 | | | ☐ |
| 命名空间 | | | ☐ |
| 类名 | | | ☐ |

### 2. 类关系

| 检查项 | 源分支 | 目标分支 | 是否一致 |
|--------|--------|----------|----------|
| 继承关系 | | | ☐ |
| 是否为内嵌类 | | | ☐ |
| 是否使用接口/抽象类 | | | ☐ |
| 是否使用工厂模式 | | | ☐ |

### 3. 接口签名

| 检查项 | 源分支 | 目标分支 | 是否一致 |
|--------|--------|----------|----------|
| 函数名称 | | | ☐ |
| 参数列表 | | | ☐ |
| 返回类型 | | | ☐ |
| 成员访问方式 | | | ☐ |

### 4. 依赖关系

| 检查项 | 源分支 | 目标分支 | 是否一致 |
|--------|--------|----------|----------|
| 包含的头文件 | | | ☐ |
| 依赖注入方式 | | | ☐ |
| 初始化流程 | | | ☐ |

---

## 常见架构差异模式

### 模式 1: 内嵌类 → 独立类

**源分支**:
```cpp
class OuterClass {
  class InnerClass {
    void method() { parent_.doSomething(); }
  };
};
```

**目标分支**:
```cpp
class IndependentClass {
  void method(Context& ctx) { ctx.doSomething(); }
};
```

**适配策略**: 修改参数传递方式，通过参数而非成员访问

### 模式 2: 直接实例化 → 工厂模式

**源分支**:
```cpp
auto obj = std::make_unique<ConcreteClass>(args);
```

**目标分支**:
```cpp
auto obj = factory->create(args);
```

**适配策略**: 在工厂实现中添加自定义逻辑

### 模式 3: 函数内实现 → 接口抽象

**源分支**:
```cpp
void process() {
  // 直接实现
}
```

**目标分支**:
```cpp
class Interface {
  virtual void process() = 0;
};
class Impl : public Interface {
  void process() override { /* 实现 */ }
};
```

**适配策略**: 将修改添加到具体实现类

---

## 验证命令

```bash
# 查看源分支中类的定义位置
git show <source_branch>:<file> | grep -n "class <ClassName>"

# 在目标分支中搜索类
git ls-tree -r <target_branch> --name-only | xargs -I {} sh -c \
  "git show <target_branch>:{} 2>/dev/null | grep -q 'class <ClassName>' && echo {}"

# 对比函数签名
git show <source_branch>:<file> | grep -A5 "void functionName"
git show <target_branch>:<file> | grep -A5 "void functionName"
```
