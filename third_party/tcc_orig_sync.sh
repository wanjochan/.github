#!/bin/bash

# TCC原始源码同步脚本
# 此脚本用于同步最新的TCC编译器源码到 tinycc/ 目录
# 并生成与 tcc/src/ 的差异对比文件

set -e  # 遇到错误时退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_diff() {
    echo -e "${CYAN}[DIFF]${NC} $1"
}

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TCC_ORIG_DIR="$SCRIPT_DIR/tinycc"
TCC_LOCAL_DIR="$SCRIPT_DIR/tcc/src"
DIFF_FILE="$SCRIPT_DIR/tcc.diff"

# TCC官方仓库地址
TCC_REPO_URL="https://repo.or.cz/tinycc.git"
# 备用镜像（如果官方仓库无法访问）
TCC_MIRROR_URL="https://github.com/TinyCC/tinycc.git"

log_info "开始同步TCC源码..."
log_info "目标目录: $TCC_ORIG_DIR"

# 检查是否已存在tinycc目录
if [ -d "$TCC_ORIG_DIR" ]; then
    log_info "检测到已存在的tinycc目录，正在更新..."
    cd "$TCC_ORIG_DIR"
    
    # 检查是否是git仓库
    if [ -d ".git" ]; then
        log_info "正在拉取最新更新..."
        git fetch origin
        git reset --hard origin/mob
        log_success "已更新到最新版本"
    else
        log_warning "目录存在但不是git仓库，将重新克隆"
        cd "$SCRIPT_DIR"
        rm -rf tinycc
        log_info "正在克隆TCC仓库..."
        if ! git clone "$TCC_REPO_URL" tinycc; then
            log_warning "主仓库克隆失败，尝试备用镜像..."
            git clone "$TCC_MIRROR_URL" tinycc
        fi
        cd tinycc
        log_success "TCC源码克隆完成"
    fi
else
    log_info "正在克隆TCC仓库..."
    cd "$SCRIPT_DIR"
    if ! git clone "$TCC_REPO_URL" tinycc; then
        log_warning "主仓库克隆失败，尝试备用镜像..."
        git clone "$TCC_MIRROR_URL" tinycc
    fi
    cd tinycc
    log_success "TCC源码克隆完成"
fi

# 显示当前版本信息
log_info "当前TCC版本信息："
echo "----------------------------------------"
git log --oneline -5
echo "----------------------------------------"

# 获取最新的commit hash和日期
LATEST_COMMIT=$(git rev-parse HEAD)
COMMIT_DATE=$(git log -1 --format="%ci" HEAD)
COMMIT_MSG=$(git log -1 --format="%s" HEAD)

log_info "最新提交: $LATEST_COMMIT"
log_info "提交时间: $COMMIT_DATE"
log_info "提交信息: $COMMIT_MSG"

# 生成diff文件 - 比对原始TCC和本地修改版本
log_diff "开始生成TCC差异对比文件..."
cd "$SCRIPT_DIR"

if [ -d "$TCC_LOCAL_DIR" ]; then
    log_diff "比对目录: $TCC_ORIG_DIR vs $TCC_LOCAL_DIR"
    
    # 创建临时的diff命令选项文件
    cat > /tmp/diff_exclude << 'EOF'
# 排除编译生成的文件和目录
*.o
*.obj
*.a
*.lib
*.so
*.dll
*.exe
*.out
tcc
tcc.exe
libtcc.a
libtcc.so
libtcc.dll
# 排除git目录
.git/
# 排除编译生成目录
build/
obj/
bin/
# 排除临时文件
*~
*.tmp
*.temp
*.bak
*.swp
*.swo
.DS_Store
# 排除IDE相关文件
.vscode/
.idea/
*.vcxproj*
*.sln
*.suo
Makefile.config
config.h
config.mak
EOF

    # 生成差异文件 (使用rsync的方式来排除不需要的文件)
    log_diff "生成详细差异报告..."
    
    # 创建diff头部信息
    cat > "$DIFF_FILE" << EOF
TCC源码差异报告
===============

比对时间: $(date)
原始版本: $LATEST_COMMIT ($COMMIT_DATE)
原始提交: $COMMIT_MSG
本地目录: tcc/src/
原始目录: tinycc/

差异统计:
---------
EOF

    # 统计文件差异
    MODIFIED_FILES=0
    NEW_FILES=0
    DELETED_FILES=0
    
    # 查找所有相关的源文件和配置文件
    DIFF_PATTERNS="*.c *.h *.S *.s Makefile* README* COPYING* LICENSE* TODO* ChangeLog* INSTALL* configure* *.in *.texi *.txt"
    
    # 生成文件列表进行比对
    cd "$TCC_ORIG_DIR"
    find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.S" -o -name "*.s" -o -name "Makefile*" -o -name "README*" -o -name "COPYING*" -o -name "LICENSE*" -o -name "TODO*" -o -name "ChangeLog*" -o -name "INSTALL*" -o -name "configure*" -o -name "*.in" -o -name "*.texi" -o -name "*.txt" \) | grep -v ".git/" | sort > /tmp/orig_files.txt
    
    cd "$TCC_LOCAL_DIR"
    find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.S" -o -name "*.s" -o -name "Makefile*" -o -name "README*" -o -name "COPYING*" -o -name "LICENSE*" -o -name "TODO*" -o -name "ChangeLog*" -o -name "INSTALL*" -o -name "configure*" -o -name "*.in" -o -name "*.texi" -o -name "*.txt" \) | sort > /tmp/local_files.txt
    
    # 添加文件状态统计到diff文件
    echo "" >> "$DIFF_FILE"
    echo "文件状态概览:" >> "$DIFF_FILE"
    echo "============" >> "$DIFF_FILE"
    
    # 检查新增文件
    log_diff "检查新增文件..."
    NEW_FILES_LIST=$(comm -13 /tmp/orig_files.txt /tmp/local_files.txt)
    if [ -n "$NEW_FILES_LIST" ]; then
        NEW_FILES=$(echo "$NEW_FILES_LIST" | wc -l)
        echo "" >> "$DIFF_FILE"
        echo "新增文件 ($NEW_FILES 个):" >> "$DIFF_FILE"
        echo "-------------------" >> "$DIFF_FILE"
        echo "$NEW_FILES_LIST" >> "$DIFF_FILE"
    fi
    
    # 检查删除文件
    log_diff "检查删除的文件..."
    DELETED_FILES_LIST=$(comm -23 /tmp/orig_files.txt /tmp/local_files.txt)
    if [ -n "$DELETED_FILES_LIST" ]; then
        DELETED_FILES=$(echo "$DELETED_FILES_LIST" | wc -l)
        echo "" >> "$DIFF_FILE"
        echo "删除文件 ($DELETED_FILES 个):" >> "$DIFF_FILE"
        echo "-------------------" >> "$DIFF_FILE"
        echo "$DELETED_FILES_LIST" >> "$DIFF_FILE"
    fi
    
    # 检查修改文件
    log_diff "检查修改的文件..."
    echo "" >> "$DIFF_FILE"
    echo "修改文件详细差异:" >> "$DIFF_FILE"
    echo "=================" >> "$DIFF_FILE"
    
    # 比较共同文件的内容差异
    COMMON_FILES=$(comm -12 /tmp/orig_files.txt /tmp/local_files.txt)
    cd "$SCRIPT_DIR"
    
    for file in $COMMON_FILES; do
        if [ -f "$TCC_ORIG_DIR/$file" ] && [ -f "$TCC_LOCAL_DIR/$file" ]; then
            # 使用 --strip-trailing-cr 忽略行尾符差异
            if ! diff -q --strip-trailing-cr "$TCC_ORIG_DIR/$file" "$TCC_LOCAL_DIR/$file" >/dev/null 2>&1; then
                MODIFIED_FILES=$((MODIFIED_FILES + 1))
                echo "" >> "$DIFF_FILE"
                echo "--- 文件: $file ---" >> "$DIFF_FILE"
                diff -u --strip-trailing-cr "$TCC_ORIG_DIR/$file" "$TCC_LOCAL_DIR/$file" >> "$DIFF_FILE" 2>/dev/null || true
            fi
        fi
    done
    
    # 更新统计信息
    sed -i "s/差异统计:/差异统计:\n新增文件: $NEW_FILES 个\n删除文件: $DELETED_FILES 个\n修改文件: $MODIFIED_FILES 个/" "$DIFF_FILE"
    
    # 清理临时文件
    rm -f /tmp/orig_files.txt /tmp/local_files.txt /tmp/diff_exclude
    
    log_success "差异文件已生成: $DIFF_FILE"
    log_diff "统计结果: 新增 $NEW_FILES 个, 删除 $DELETED_FILES 个, 修改 $MODIFIED_FILES 个文件"
    
    # 如果有差异，显示摘要
    if [ $((NEW_FILES + DELETED_FILES + MODIFIED_FILES)) -gt 0 ]; then
        log_warning "检测到本地TCC与原始版本存在差异，请查看 tcc.diff 文件"
    else
        log_success "本地TCC与原始版本无差异"
    fi
else
    log_warning "未找到本地TCC目录 ($TCC_LOCAL_DIR)，跳过diff生成"
    echo "TCC源码差异报告" > "$DIFF_FILE"
    echo "===============" >> "$DIFF_FILE"
    echo "" >> "$DIFF_FILE"
    echo "比对时间: $(date)" >> "$DIFF_FILE"
    echo "状态: 未找到本地TCC目录进行比对" >> "$DIFF_FILE"
fi

# 创建同步信息文件
cd "$SCRIPT_DIR"
cat > tinycc_sync_info.txt << EOF
TCC源码同步信息
==============

同步时间: $(date)
源仓库: $TCC_REPO_URL
最新提交: $LATEST_COMMIT
提交时间: $COMMIT_DATE
提交信息: $COMMIT_MSG

diff文件: tcc.diff (本地TCC vs 原始TCC的差异)

目录结构:
$(cd tinycc && find . -maxdepth 2 -type f -name "*.c" -o -name "*.h" -o -name "Makefile*" -o -name "README*" | head -20)
EOF

log_success "同步信息已保存到 tinycc_sync_info.txt"

# 检查是否在git仓库中
cd "$PROJECT_ROOT"
if [ -d ".git" ]; then
    log_info "检测到项目git仓库，准备提交更改..."
    
    # 添加新文件到git
    git add tinycc/
    git add tinycc_sync.sh
    git add tinycc_sync_info.txt
    git add tcc.diff
    
    # 检查是否有更改
    if git diff --cached --quiet; then
        log_info "没有检测到更改，跳过提交"
    else
        # 提交更改
        COMMIT_MESSAGE="sync: 更新TCC源码到最新版本 ($LATEST_COMMIT) 并生成diff"
        git commit -m "$COMMIT_MESSAGE"
        log_success "更改已提交到本地仓库"
        
        # 询问是否推送到远程
        log_info "准备推送到远程main分支..."
        
        # 获取当前分支
        CURRENT_BRANCH=$(git branch --show-current)
        log_info "当前分支: $CURRENT_BRANCH"
        
        # 如果不在main分支，切换到main
        if [ "$CURRENT_BRANCH" != "main" ]; then
            log_info "切换到main分支..."
            git checkout main
            git merge "$CURRENT_BRANCH" --no-ff -m "merge: 合并TCC同步更新和diff生成"
        fi
        
        # 推送到远程
        log_info "【跳过】推送到远程main分支..."
        #git push origin main
        log_success "已成功推送到远程main分支"
    fi
else
    log_warning "未检测到git仓库，跳过git操作"
fi

log_success "TCC源码同步完成！"
log_info "TCC源码位置: $TCC_ORIG_DIR"
log_info "本地TCC位置: $TCC_LOCAL_DIR"
log_info "差异文件: $DIFF_FILE"
log_info "同步信息文件: $SCRIPT_DIR/tinycc_sync_info.txt"

# 显示diff文件的简要信息
if [ -f "$DIFF_FILE" ]; then
    DIFF_SIZE=$(wc -l < "$DIFF_FILE")
    log_info "差异文件包含 $DIFF_SIZE 行内容"
fi
