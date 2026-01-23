#!/bin/bash
# 收集 cherry-pick 冲突信息的脚本
# 用法: ./collect_conflicts.sh [output_file]

set -e

OUTPUT_FILE="${1:-conflict_info.txt}"

echo "=== Cherry-pick 冲突信息收集 ===" | tee "$OUTPUT_FILE"
echo "时间: $(date)" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# 检查是否处于 cherry-pick 状态
if [ ! -f ".git/CHERRY_PICK_HEAD" ]; then
    echo "错误: 当前不处于 cherry-pick 状态" | tee -a "$OUTPUT_FILE"
    exit 1
fi

COMMIT_HASH=$(cat .git/CHERRY_PICK_HEAD)
echo "提交: $COMMIT_HASH" | tee -a "$OUTPUT_FILE"
echo "提交信息: $(git log --oneline -1 $COMMIT_HASH)" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# 收集冲突文件列表
echo "=== 冲突文件列表 ===" | tee -a "$OUTPUT_FILE"
CONFLICT_FILES=$(git diff --name-only --diff-filter=U)

if [ -z "$CONFLICT_FILES" ]; then
    echo "没有发现冲突文件" | tee -a "$OUTPUT_FILE"
    exit 0
fi

echo "$CONFLICT_FILES" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# 统计冲突文件数量
FILE_COUNT=$(echo "$CONFLICT_FILES" | wc -l)
echo "冲突文件数量: $FILE_COUNT" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# 收集每个文件的冲突位置
echo "=== 冲突位置详情 ===" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

for file in $CONFLICT_FILES; do
    echo "### $file" | tee -a "$OUTPUT_FILE"
    
    # 获取冲突标记的行号
    conflict_lines=$(grep -n "^<<<<<<< HEAD\|^=======\|^>>>>>>>" "$file" 2>/dev/null || true)
    
    if [ -n "$conflict_lines" ]; then
        echo "$conflict_lines" | tee -a "$OUTPUT_FILE"
        
        # 统计冲突块数量
        block_count=$(grep -c "^<<<<<<< HEAD" "$file" 2>/dev/null || echo "0")
        echo "冲突块数量: $block_count" | tee -a "$OUTPUT_FILE"
    else
        echo "无法读取冲突标记" | tee -a "$OUTPUT_FILE"
    fi
    
    echo "" | tee -a "$OUTPUT_FILE"
done

echo "=== 收集完成 ===" | tee -a "$OUTPUT_FILE"
echo "结果已保存到: $OUTPUT_FILE"
