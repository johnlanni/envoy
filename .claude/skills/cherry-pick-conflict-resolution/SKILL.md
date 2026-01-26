---
name: cherry-pick-conflict-resolution
description: 跨版本 cherry-pick 冲突解决流程。当用户需要将补丁从一个分支迁移到另一个分支，特别是存在架构差异的场景时使用此技能。
license: MIT
compatibility:
  - git
  - bash
metadata:
  version: "1.1"
  author: "zhangty"
  created: "2026-01-22"
  updated: "2026-01-26"
  changelog:
    - "1.1: 新增 Phase 0 准备阶段（创建工作分支）；强化 Phase 2 提示，禁止跳过审核流程"
---

# Cherry-pick 冲突解决技能

本技能提供完整的跨版本 cherry-pick 冲突解决流程，特别适用于存在架构差异的大型代码库迁移场景。

## 使用时机

- 用户需要将 commit 从一个分支 cherry-pick 到另一个分支
- Cherry-pick 产生了冲突需要解决
- 两个分支之间可能存在架构重构差异

## 流程概览

```
Phase 0: 准备 → Phase 1: 探测 → Phase 2: 文档生成 
                                      ↓
                              ┌──────────────────┐
                              │  Phase 3: 审核   │◄──┐
                              └────────┬─────────┘   │
                                       ↓             │ 迭代循环
                              ┌──────────────────┐   │ (每个可疑项)
                              │  Phase 4: 验证   │   │
                              └────────┬─────────┘   │
                                       ↓             │
                              ┌──────────────────┐   │
                              │  Phase 5: 完善   │───┘
                              └────────┬─────────┘
                                       ↓ 用户确认
                              ┌──────────────────┐
                              │  Phase 6: 执行   │
                              └────────┬─────────┘
                                       ↓
                              ┌──────────────────┐
                              │  Phase 7: 编译   │◄──┐
                              └────────┬─────────┘   │ 迭代修复
                                       ↓             │
                              ┌──────────────────┐   │
                              │  编译修复循环    │───┘
                              └────────┬─────────┘
                                       ↓ 编译成功
                              ┌──────────────────┐
                              │  Phase 8: 单测   │◄──┐
                              └────────┬─────────┘   │ 迭代修复
                                       ↓             │
                              ┌──────────────────┐   │
                              │  单测修复循环    │───┘
                              └────────┬─────────┘
                                       ↓ 测试通过
                              ┌──────────────────┐
                              │  Phase 9: 归档   │
                              └────────┬─────────┘
                                       ↓
                                    完成 ✅
```

## ⚠️ 关键原则

> **禁止跳过 Phase 2 和 Phase 3！**
>
> 在解决任何冲突之前，**必须**先完成以下步骤：
> 1. 生成 `CHERRY_PICK_CONFLICT_RESOLUTION.md` 文档
> 2. 等待用户审核确认
> 3. 用户明确表示"确认执行"后才能进入 Phase 6
>
> **绝对禁止**在用户审核前直接开始解决冲突！

## 指令

### Phase 0: 准备阶段

> **重要**: 在开始 cherry-pick 之前，必须创建一个新的工作分支，避免直接在目标分支上操作。

1. 确认当前分支状态：
   ```bash
   git status
   git branch --show-current
   ```

2. 切换到目标分支并创建工作分支：
   ```bash
   git checkout <target_branch>
   git checkout -b cherry-pick/<commit_short>-to-<target_branch>
   # 例如: git checkout -b cherry-pick/012aa759-to-1.36.4-merge
   ```

3. 确认工作分支创建成功：
   ```bash
   git branch --show-current
   # 应输出: cherry-pick/<commit_short>-to-<target_branch>
   ```

**分支命名规范**: `cherry-pick/<commit_short_hash>-to-<target_branch>`

---

### Phase 1: 探测阶段

1. 在工作分支上尝试 cherry-pick：
   ```bash
   git cherry-pick <commit_hash>
   ```

2. 收集冲突信息（使用脚本自动收集）：
   ```bash
   # 运行冲突信息收集脚本
   bash scripts/collect_conflicts.sh
   
   # 或手动收集：
   # git diff --name-only --diff-filter=U
   # grep -n "^<<<<<<< HEAD\|^=======\|^>>>>>>>" <file>
   ```
   
   脚本会输出：提交信息、冲突文件列表、每个文件的冲突位置和冲突块数量。

### Phase 2: 文档生成阶段

> ⛔ **强制要求**: 此阶段必须生成文档并等待用户审核！
>
> **禁止行为**:
> - ❌ 跳过文档生成直接解决冲突
> - ❌ 在用户确认前修改任何冲突文件
> - ❌ 假设方案正确而不等待审核
>
> **必须行为**:
> - ✅ 生成完整的 `CHERRY_PICK_CONFLICT_RESOLUTION.md` 文档
> - ✅ 在文档末尾明确提示用户审核
> - ✅ 等待用户明确表示"确认执行"后才进入 Phase 6

生成冲突解决方案文档 `CHERRY_PICK_CONFLICT_RESOLUTION.md`，包含：
- 概述（提交信息、源分支、目标分支、冲突文件数）
- 冲突文件列表（文件路径、冲突位置、冲突类型、风险等级）
- **每个冲突点的详细分析**（必须包含冲突内容、分析、解决方案）
- 风险评估和验证清单
- **文档末尾必须包含**：`**请审核以上方案，确认无误后回复"确认执行"开始解决冲突。**`

使用文档模板：`references/CONFLICT_DOC_TEMPLATE.md`

**文档生成后的状态检查**:
- [ ] 所有冲突文件都已分析
- [ ] 每个冲突点都有明确的解决方案
- [ ] 文档末尾有审核提示
- [ ] **已通知用户进行审核**

### Phase 3-5: 审核-验证-完善 迭代循环

> **注意**: Phase 3、4、5 是一个迭代循环，针对每个冲突项可能多次执行，直到用户确认所有判断正确后才进入 Phase 6。

```
┌─────────────────────────────────────────────────┐
│           Phase 3: 人工审核                      │
│  用户审核文档，标记可疑判断                        │
└──────────────────┬──────────────────────────────┘
                   │ 发现可疑项
                   ▼
┌─────────────────────────────────────────────────┐
│           Phase 4: 深度验证                      │
│  对可疑项进行代码对比验证                         │
└──────────────────┬──────────────────────────────┘
                   │ 验证完成
                   ▼
┌─────────────────────────────────────────────────┐
│           Phase 5: 文档完善                      │
│  更新文档中的错误判断                             │
└──────────────────┬──────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │ 还有其他可疑项？    │
         └─────────┬─────────┘
              是 ↙   ↘ 否
    返回 Phase 3      进入 Phase 6
```

#### Phase 3: 人工审核

等待用户审核文档，特别关注：
- ⚠️ "新增函数" 的判断 → 需验证目标分支是否已有
- ⚠️ "采用 patch 版本" 的建议 → 需验证架构差异
- ⚠️ 涉及核心逻辑的冲突 → 需深度验证

用户可能的反馈：
- 指出某个判断需要验证
- 确认所有判断正确，可以开始执行

#### Phase 4: 深度验证

当用户标记可疑判断时：

1. 终止当前 cherry-pick（如果还在冲突状态）：
   ```bash
   git cherry-pick --abort
   ```

2. 切换到源分支验证：
   ```bash
   git checkout <source_branch>
   git show <commit_hash> -- <file>
   ```

3. 对比目标分支架构：
   ```bash
   git show <target_branch>:<file>
   
   # 搜索关键类/函数的位置
   git show <target_branch>:<file> | grep -n "<keyword>"
   ```

4. 检查架构差异（参考 `references/ARCHITECTURE_CHECKLIST.md`）：
   - 类定义位置是否相同？
   - 接口签名是否一致？
   - 是否有代码重构/移动？

#### Phase 5: 文档完善

根据验证结果更新文档：
- 修正错误的判断
- 补充架构差异说明
- 将解决方案分类：✅ 可直接合并 / ⚠️ 需手动合并 / 🔧 需额外适配
- 更新风险评估

**完成后返回 Phase 3**，等待用户确认或提出新的可疑项。

#### 退出条件

当用户明确表示"确认可以开始执行"或"所有判断已确认"时，进入 Phase 6。

### Phase 6: 执行阶段

1. 重新发起 cherry-pick
2. 按文档逐个解决冲突：
   ```bash
   git checkout --ours <file>    # 采用 HEAD 版本
   git checkout --theirs <file>  # 采用 Incoming 版本
   git add <file>                # 标记已解决
   ```
3. 完成 cherry-pick：
   ```bash
   git cherry-pick --continue
   ```

### Phase 7: 编译验证阶段

> **注意**: 完成 cherry-pick 后必须进行编译验证，确保代码能正确编译。

#### 7.1 识别编译目标

1. 获取非测试代码的改动文件：
   ```bash
   git diff HEAD~1 --name-only | grep -v "^test/" | grep -v "_test\." | grep -v "_test_"
   ```

2. 找到对应的 BUILD 文件：
   ```bash
   # 对于每个目录，检查 BUILD 文件
   ls <directory>/BUILD
   ```

3. 确定编译目标（常见模式）：
   - `source/common/http/` → `//source/common/http:conn_manager_lib`
   - `source/common/router/` → `//source/common/router:config_lib`
   - `source/extensions/filters/network/http_connection_manager/` → `//source/extensions/filters/network/http_connection_manager:config`

#### 7.2 执行编译

> ⚠️ **重要**: 不要使用 `-c opt` 参数，这会导致优化编译非常慢。默认的 fastbuild 模式足够验证编译正确性。

```bash
# 确保代理开启（如果需要）
startProxy  # 或其他代理命令

# 执行编译（不使用 -c opt，使用默认 fastbuild 模式）
bazel build --config=clang <targets>

# 示例：编译 contrib 扩展
bazel build --config=clang //contrib/<extension_name>/...
```

#### 7.3 处理编译错误

如果编译失败，需要：

1. **生成编译修复文档** (`COMPILATION_FIX_PLAN.md`)：
   - 记录所有编译错误
   - 分析错误原因（接口不兼容、语法差异、缺失方法等）
   - 提出修复方案

2. **等待用户确认**后开始修复：
   - 用户审核修复计划
   - 用户指示"开始修复"后执行修复

3. **迭代修复**：
   ```
   编译 → 收集错误 → 修复 → 重新编译 → 直到成功
   ```

#### 7.4 编译成功

编译成功后进入 Phase 8 单元测试验证。

### Phase 8: 单元测试验证阶段

> **注意**: 编译成功后必须运行相关单元测试，确保修改没有引入回归问题。

#### 8.1 识别相关测试

测试文件来源有**两类**，都需要检查：

**类型 A: 直接被 cherry-pick 修改的测试文件**

使用 git 命令获取被修改的测试文件：
```bash
# 查看所有被修改的测试文件
git diff --name-only HEAD~1 | grep "^test/"

# 或查看 cherry-pick 提交修改的所有文件
git show --name-only HEAD | grep "^test/"
```

这些文件可能包含 patch 新增的测试代码，需要适配目标分支架构。

**类型 B: 源代码对应的测试文件**

根据修改的源代码文件找到对应的测试文件：
```bash
# 常见的测试文件命名规则
# source/common/http/conn_manager_impl.cc → test/common/http/conn_manager_impl_test.cc
# source/common/router/config_impl.cc → test/common/router/config_impl_test.cc
```

**完整测试列表示例**：
```
# 类型 A: 直接被修改的测试文件（从 git diff --name-only HEAD~1 获取）
test/common/<module>/<test_file>_test.cc     # 直接被 patch 修改的测试
test/common/<module>/<other_test>_test.cc    # ← 容易遗漏！
test/mocks/<module>/mocks.h
test/mocks/<module>/mocks.cc

# 类型 B: 源代码对应的测试（可能与类型 A 重叠）
# source/common/<module>/<impl>.cc → test/common/<module>/<impl>_test.cc
```

**确定测试目标**：
```bash
# 常见模式
# source/common/http/ → //test/common/http:<test_name>
# source/common/router/ → //test/common/router:<test_name>
# source/extensions/filters/network/http_connection_manager/ → //test/extensions/filters/network/http_connection_manager:config_test
```

#### 8.2 执行单元测试

> ⚠️ **关键**: 必须测试 **所有** 被修改的测试文件，不仅仅是有冲突的！

**步骤 1**: 先编译所有测试目标（检查编译错误）
```bash
# 编译所有被修改的测试
bazel build --config=clang \
  //test/common/<module1>:<test1> \
  //test/common/<module2>:<test2> \
  ...
```

**步骤 2**: 运行测试
```bash
# 运行相关测试
bazel test --config=clang <test_targets>

# 例如
bazel test --config=clang \
  //test/common/<module1>:<test1> \
  //test/common/<module2>:<test2>
```

**注意**: 有些测试文件可能在冲突解决阶段没有冲突，但仍包含需要适配目标分支架构的代码。这些"无冲突"的测试文件**容易被遗漏**！必须用 `git diff --name-only HEAD~1` 完整检查所有测试文件。

#### 8.3 处理测试失败

如果测试失败，需要：

1. **分析失败原因**：
   ```bash
   # 查看测试日志
   cat <bazel_cache_path>/testlogs/<target>/test.log
   
   # 常见失败信息
   # - EXPECT_CALL(...) Expected: to be called N times, Actual: never called
   # - Mock方法签名不匹配
   # - 断言失败
   ```

2. **生成测试修复文档** (`TEST_FIX_PLAN.md`)：
   - 列出所有失败的测试用例
   - 分析失败根因
   - 提出修复方案

3. **常见测试失败类型**：

   **A. 编译时错误（测试文件本身无法编译）**

   **B. 运行时错误（编译通过但测试失败）**

5. **等待用户确认**后开始修复

6. **迭代修复**：
   ```
   测试 → 收集失败 → 修复 → 重新测试 → 直到通过
   ```

#### 8.4 测试通过

所有测试通过后：
1. 更新测试修复文档，标记所有问题已解决
2. 提交所有修复：
   ```bash
   git add -A
   git commit --amend --no-edit  # 合并到 cherry-pick 提交
   # 或
   git commit -m "Fix: adapt tests to target branch interface changes"
   ```
3. 进入 Phase 9 文档归档

### Phase 9: 文档归档阶段

> **目的**: 将本次 cherry-pick 过程中产生的所有文档归档保存，便于未来排查问题、回溯决策依据。

#### 9.1 归档目录结构

```
docs/cherry-pick-archives/
└── YYYY-MM-DD_<commit_short>_<source>_to_<target>/
    ├── SUMMARY.md                           # 归档总结（必需）
    ├── CHERRY_PICK_CONFLICT_RESOLUTION.md   # 冲突解决方案（必需）
    ├── COMPILATION_FIX_PLAN.md              # 编译修复计划（如有）
    ├── TEST_FIX_PLAN.md                     # 单测修复计划（如有）
    ├── commit_info.txt                      # 原始提交信息
    └── modified_files.txt                   # 修改的文件列表
```

**目录命名规范**: `YYYY-MM-DD_<commit_short_hash>_<source_branch>_to_<target_branch>`

示例: `2026-01-22_abc12345_source-branch_to_target-branch`

#### 9.2 执行归档

**方式一：使用归档脚本（推荐）**

```bash
bash scripts/archive_docs.sh <commit_hash> <source_branch> <target_branch>

# 示例
bash scripts/archive_docs.sh <commit_hash> <source_branch> <target_branch>
```

**方式二：手动归档**

```bash
# 1. 创建归档目录
DATE=$(date +%Y-%m-%d)
COMMIT_SHORT=$(git rev-parse --short HEAD~1)  # cherry-pick 的原始提交
ARCHIVE_DIR="docs/cherry-pick-archives/${DATE}_${COMMIT_SHORT}_<source>_to_<target>"
mkdir -p "$ARCHIVE_DIR"

# 2. 保存提交信息
git show --stat HEAD > "$ARCHIVE_DIR/commit_info.txt"

# 3. 保存修改文件列表
git diff HEAD~1 --name-only > "$ARCHIVE_DIR/modified_files.txt"

# 4. 移动文档到归档目录
mv CHERRY_PICK_CONFLICT_RESOLUTION.md "$ARCHIVE_DIR/"
[ -f COMPILATION_FIX_PLAN.md ] && mv COMPILATION_FIX_PLAN.md "$ARCHIVE_DIR/"
[ -f TEST_FIX_PLAN.md ] && mv TEST_FIX_PLAN.md "$ARCHIVE_DIR/"

# 5. 生成总结文档（参考模板）
# 使用 references/ARCHIVE_SUMMARY_TEMPLATE.md 模板
```

#### 9.3 归档总结文档 (SUMMARY.md)

总结文档应包含：

```markdown
# Cherry-pick 归档总结

## 基本信息
- **日期**: YYYY-MM-DD
- **原始提交**: <commit_hash>
- **源分支**: <source_branch>
- **目标分支**: <target_branch>
- **操作人**: <operator>

## 冲突概览
- 冲突文件数: N
- 编译问题: 是/否
- 单测问题: 是/否

## 关键决策
1. <决策1>: <原因>
2. <决策2>: <原因>

## 注意事项
- <需要注意的点>

## 相关文件
- [ ] CHERRY_PICK_CONFLICT_RESOLUTION.md
- [ ] COMPILATION_FIX_PLAN.md (如有)
- [ ] TEST_FIX_PLAN.md (如有)
```

#### 9.4 提交归档

```bash
git add docs/cherry-pick-archives/
git commit -m "docs: archive cherry-pick resolution for <commit_short>"
```

#### 9.5 增量更新归档

> ⚠️ **重要**: 当后续发现遗漏问题并进行修复时，必须同步更新归档文档！

**触发场景**：
- 发现遗漏的测试文件需要修复
- 发现新的编译问题
- 用户指出之前的分析有遗漏或错误

**更新流程**：

1. **修复问题后，立即更新对应的归档文档**：
   ```bash
   # 更新测试修复计划
   vim docs/cherry-pick-archives/<archive_dir>/TEST_FIX_PLAN.md
   
   # 或更新编译修复计划
   vim docs/cherry-pick-archives/<archive_dir>/COMPILATION_FIX_PLAN.md
   ```

2. **在 SUMMARY.md 中记录增量更新**：
   ```markdown
   ## 增量更新记录
   
   | 日期 | 更新内容 | 原因 |
   |------|---------|------|
   | YYYY-MM-DD | 新增 <test_file> 修复 | 初次归档时遗漏该测试文件 |
   ```

3. **提交更新**：
   ```bash
   git add docs/cherry-pick-archives/<archive_dir>/
   git commit -m "docs: update archive with additional fixes for <issue>"
   ```

**原则**: 归档文档应始终反映 cherry-pick 的**最终完整状态**，而不仅仅是初次归档时的状态。

## 常见陷阱

| 陷阱 | 错误做法 | 正确做法 |
|------|---------|---------|
| ⛔ **跳过审核流程** | 直接解决冲突不生成文档 | **必须**先生成文档等待用户审核确认 |
| ⛔ **直接在目标分支操作** | 在 main/target 分支直接 cherry-pick | 创建工作分支 `cherry-pick/<commit>-to-<target>` |
| 假设新增函数 | HEAD 为空就认为是新增 | 检查目标分支其他文件是否有该函数 |
| 忽略架构重构 | 直接采用 patch 代码 | 对比两个版本的架构 |
| 接口签名差异 | 直接复制函数实现 | 检查接口是否需要适配 |
| 跳过编译验证 | 完成冲突解决就提交 | 必须编译验证后再提交 |
| 跳过单测验证 | 编译成功就提交 | 必须运行单测后再提交 |
| 遗漏测试文件 | 只测试有冲突的测试文件 | 测试 **所有** 被修改的测试文件 |
| Proto 字段冲突 | 合并时保留两边相同字段 | 检查字段号是否重复 |
| 忽略条件编译 | 测试代码不考虑 HIGRESS 等宏 | 测试也需添加条件编译分支 |
| 类型移除 | 继续使用源分支的类型 | 检查类型在目标分支是否存在 |
| 跳过文档归档 | 完成后直接删除临时文档 | 归档保存以便未来排查 |
| 归档不同步 | 后续修复不更新归档 | 每次修复后同步更新归档文档 |

## 参考资料

- 冲突解决文档模板：`references/CONFLICT_DOC_TEMPLATE.md`
- 架构对比检查清单：`references/ARCHITECTURE_CHECKLIST.md`
- 命令速查表：`references/COMMANDS.md`
- 编译修复文档模板：`references/COMPILATION_FIX_TEMPLATE.md`
- 单测修复文档模板：`references/TEST_FIX_TEMPLATE.md`
- 归档总结模板：`references/ARCHIVE_SUMMARY_TEMPLATE.md`

## 脚本

- **冲突信息收集**: `scripts/collect_conflicts.sh`
  - 用法: `bash scripts/collect_conflicts.sh [output_file]`
  - 功能: 自动收集当前 cherry-pick 的冲突信息，包括提交信息、冲突文件列表、每个文件的冲突位置
  - 在 Phase 1 探测阶段使用

- **文档归档**: `scripts/archive_docs.sh`
  - 用法: `bash scripts/archive_docs.sh <original_commit> <source_branch> <target_branch>`
  - 功能: 自动归档本次 cherry-pick 产生的所有文档，生成总结文档
  - 在 Phase 9 归档阶段使用
  - 示例: `bash scripts/archive_docs.sh <commit_hash> <source_branch> <target_branch>`

## 输出文档

| 阶段 | 文档 | 说明 |
|------|------|------|
| Phase 2 | `CHERRY_PICK_CONFLICT_RESOLUTION.md` | 冲突解决方案 |
| Phase 7 | `COMPILATION_FIX_PLAN.md` | 编译修复计划（如有错误）|
| Phase 8 | `TEST_FIX_PLAN.md` | 单测修复计划（如有失败）|