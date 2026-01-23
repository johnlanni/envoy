#!/bin/bash
#
# Cherry-pick 文档归档脚本
# 用法: bash archive_docs.sh <original_commit> <source_branch> <target_branch>
#
# 示例: bash archive_docs.sh 9f11788ab6f2 1.27.7-merge 1.36.4-merge
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 参数检查
if [ $# -lt 3 ]; then
    echo -e "${RED}错误: 缺少参数${NC}"
    echo "用法: $0 <original_commit> <source_branch> <target_branch>"
    echo "示例: $0 9f11788ab6f2 1.27.7-merge 1.36.4-merge"
    exit 1
fi

ORIGINAL_COMMIT=$1
SOURCE_BRANCH=$2
TARGET_BRANCH=$3

# 获取短提交哈希
COMMIT_SHORT=$(echo "$ORIGINAL_COMMIT" | cut -c1-8)
DATE=$(date +%Y-%m-%d)

# 清理分支名中的斜杠（用于目录名）
SOURCE_CLEAN=$(echo "$SOURCE_BRANCH" | tr '/' '-')
TARGET_CLEAN=$(echo "$TARGET_BRANCH" | tr '/' '-')

# 创建归档目录
ARCHIVE_DIR="docs/cherry-pick-archives/${DATE}_${COMMIT_SHORT}_${SOURCE_CLEAN}_to_${TARGET_CLEAN}"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Cherry-pick 文档归档${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "原始提交: $ORIGINAL_COMMIT"
echo "源分支: $SOURCE_BRANCH"
echo "目标分支: $TARGET_BRANCH"
echo "归档目录: $ARCHIVE_DIR"
echo ""

# 创建目录
mkdir -p "$ARCHIVE_DIR"
echo -e "${GREEN}✓${NC} 创建归档目录"

# 保存提交信息
git show --stat "$ORIGINAL_COMMIT" > "$ARCHIVE_DIR/commit_info.txt" 2>/dev/null || \
git show --stat HEAD > "$ARCHIVE_DIR/commit_info.txt"
echo -e "${GREEN}✓${NC} 保存提交信息"

# 保存修改文件列表
git diff HEAD~1 --name-only > "$ARCHIVE_DIR/modified_files.txt" 2>/dev/null || \
echo "无法获取修改文件列表" > "$ARCHIVE_DIR/modified_files.txt"
echo -e "${GREEN}✓${NC} 保存修改文件列表"

# 移动文档
DOCS_MOVED=0

if [ -f "CHERRY_PICK_CONFLICT_RESOLUTION.md" ]; then
    mv CHERRY_PICK_CONFLICT_RESOLUTION.md "$ARCHIVE_DIR/"
    echo -e "${GREEN}✓${NC} 归档 CHERRY_PICK_CONFLICT_RESOLUTION.md"
    DOCS_MOVED=$((DOCS_MOVED + 1))
else
    echo -e "${YELLOW}!${NC} 未找到 CHERRY_PICK_CONFLICT_RESOLUTION.md"
fi

if [ -f "COMPILATION_FIX_PLAN.md" ]; then
    mv COMPILATION_FIX_PLAN.md "$ARCHIVE_DIR/"
    echo -e "${GREEN}✓${NC} 归档 COMPILATION_FIX_PLAN.md"
    DOCS_MOVED=$((DOCS_MOVED + 1))
fi

if [ -f "TEST_FIX_PLAN.md" ]; then
    mv TEST_FIX_PLAN.md "$ARCHIVE_DIR/"
    echo -e "${GREEN}✓${NC} 归档 TEST_FIX_PLAN.md"
    DOCS_MOVED=$((DOCS_MOVED + 1))
fi

# 生成总结文档
SUMMARY_FILE="$ARCHIVE_DIR/SUMMARY.md"
cat > "$SUMMARY_FILE" << EOF
# Cherry-pick 归档总结

## 基本信息

| 项目 | 值 |
|------|-----|
| **归档日期** | $DATE |
| **原始提交** | \`$ORIGINAL_COMMIT\` |
| **源分支** | \`$SOURCE_BRANCH\` |
| **目标分支** | \`$TARGET_BRANCH\` |
| **操作人** | $(git config user.name) |

## 提交信息

\`\`\`
$(head -20 "$ARCHIVE_DIR/commit_info.txt")
\`\`\`

## 修改文件概览

共修改 $(wc -l < "$ARCHIVE_DIR/modified_files.txt") 个文件：

\`\`\`
$(cat "$ARCHIVE_DIR/modified_files.txt")
\`\`\`

## 冲突解决概览

> 请补充以下信息：

- **冲突文件数**: _待填写_
- **是否有编译问题**: $([ -f "$ARCHIVE_DIR/COMPILATION_FIX_PLAN.md" ] && echo "是" || echo "否")
- **是否有单测问题**: $([ -f "$ARCHIVE_DIR/TEST_FIX_PLAN.md" ] && echo "是" || echo "否")

## 关键决策

> 请补充本次 cherry-pick 中做出的重要决策：

1. _待填写_

## 注意事项

> 请补充未来维护者需要注意的事项：

- _待填写_

## 归档文件清单

- [x] SUMMARY.md (本文档)
- [x] commit_info.txt
- [x] modified_files.txt
$([ -f "$ARCHIVE_DIR/CHERRY_PICK_CONFLICT_RESOLUTION.md" ] && echo "- [x] CHERRY_PICK_CONFLICT_RESOLUTION.md" || echo "- [ ] CHERRY_PICK_CONFLICT_RESOLUTION.md (未找到)")
$([ -f "$ARCHIVE_DIR/COMPILATION_FIX_PLAN.md" ] && echo "- [x] COMPILATION_FIX_PLAN.md")
$([ -f "$ARCHIVE_DIR/TEST_FIX_PLAN.md" ] && echo "- [x] TEST_FIX_PLAN.md")

---

*归档时间: $(date '+%Y-%m-%d %H:%M:%S')*
EOF

echo -e "${GREEN}✓${NC} 生成总结文档 SUMMARY.md"

# 输出结果
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}归档完成!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "归档目录: $ARCHIVE_DIR"
echo "归档文件: $((DOCS_MOVED + 3)) 个"
echo ""
echo -e "${YELLOW}下一步操作:${NC}"
echo "1. 编辑 $SUMMARY_FILE 补充关键决策和注意事项"
echo "2. 提交归档:"
echo "   git add $ARCHIVE_DIR"
echo "   git commit -m \"docs: archive cherry-pick resolution for $COMMIT_SHORT\""
echo ""
