# Git 命令速查表

## Phase 1: 探测阶段

```bash
# 切换到目标分支
git checkout <target_branch>

# 尝试 cherry-pick
git cherry-pick <commit_hash>

# 列出所有冲突文件
git diff --name-only --diff-filter=U

# 查看冲突位置
grep -n "^<<<<<<< HEAD\|^=======\|^>>>>>>>" <file>

# 查看提交信息
git show <commit_hash> --stat

# 查看提交历史上下文
git log --oneline -5 <commit_hash>
```

## Phase 4: 深度验证阶段

```bash
# 终止当前 cherry-pick
git cherry-pick --abort

# 切换到源分支
git checkout <source_branch>

# 查看提交对特定文件的修改
git show <commit_hash> -- <file>

# 查看提交前的代码状态
git show <parent_commit>:<file>

# 查看目标分支中的文件内容
git show <target_branch>:<file>

# 在文件中搜索关键字
git show <branch>:<file> | grep -n "<keyword>"

# 在整个分支中搜索包含关键字的文件
git ls-tree -r <branch> --name-only | xargs -I {} sh -c \
  "git show <branch>:{} 2>/dev/null | grep -q '<keyword>' && echo {}"

# 查看特定行范围
git show <branch>:<file> | sed -n '<start>,<end>p'
```

## Phase 6: 执行阶段

```bash
# 重新发起 cherry-pick
git cherry-pick <commit_hash>

# 采用 HEAD (目标分支) 版本
git checkout --ours <file>

# 采用 Incoming (源提交) 版本
git checkout --theirs <file>

# 手动编辑后标记已解决
git add <file>

# 查看当前冲突状态
git status

# 继续 cherry-pick
git cherry-pick --continue

# 放弃 cherry-pick
git cherry-pick --abort

# 跳过当前提交
git cherry-pick --skip
```

## 辅助命令

```bash
# 查看两个分支的差异
git diff <branch1>..<branch2> -- <file>

# 查看文件的修改历史
git log --oneline -10 -- <file>

# 查看谁修改了特定行
git blame <file> -L <start>,<end>

# 查看分支列表
git branch -a | grep <pattern>

# 创建临时分支测试
git checkout -b test-cherry-pick
```
