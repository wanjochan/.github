#!/bin/bash

# TCC 辅助脚本 (Linux/Unix)
# 智能选择和调用合适的 TCC 二进制文件，自动配置路径

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TCC_BIN_DIR="$SCRIPT_DIR/bin"

# 检测当前系统架构
detect_arch() {
    local arch="$(uname -m)"
    case "$arch" in
        x86_64|amd64)
            echo "x86_64"
            ;;
        i?86)
            echo "i386"
            ;;
        aarch64|arm64)
            echo "aarch64"
            ;;
        arm*)
            echo "arm"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# 检测当前操作系统
detect_os() {
    case "$OSTYPE" in
        linux-*)
            echo "lnx"
            ;;
        darwin*)
            echo "osx"
            ;;
        cygwin|msys|win32)
            echo "exe"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# 选择最佳的 TCC 二进制文件
find_best_tcc() {
    local arch="$(detect_arch)"
    local os="$(detect_os)"
    
    # 架构映射
    local tcc_arch=""
    local bits=""
    case "$arch" in
        x86_64)
            tcc_arch="x86"
            bits="64"
            ;;
        i386)
            tcc_arch="x86"
            bits="32"
            ;;
        aarch64)
            tcc_arch="arm"
            bits="64"
            ;;
        arm)
            tcc_arch="arm"
            bits="32"
            ;;
        *)
            echo "错误: 不支持的架构 $arch" >&2
            return 1
            ;;
    esac
    
    # 构建文件名: tcc_{arch}_{bits}.{bin|exe}
    local filename="tcc_${tcc_arch}_${bits}.${os}"
    local tcc_path="$TCC_BIN_DIR/$filename"
    
    echo "寻找TCC二进制: $filename (架构:$arch, 位数:${bits}bit, 系统:$os)" >&2
    
    if [ -f "$tcc_path" ]; then
        echo "$tcc_path"
        return 0
    fi
    
    # 回退选项：尝试 x86_64 Linux
    if [ "$os" = "lnx" ] && [ "$arch" != "x86_64" ]; then
        local fallback="$TCC_BIN_DIR/tcc_x86_64.lnx"
        if [ -f "$fallback" ]; then
            echo "$fallback"
            return 0
        fi
    fi
    
    return 1
}

# 获取系统头文件和库文件路径
get_system_paths() {
    local include_paths=""
    local lib_paths=""
    
    # 添加 TCC 自带的头文件
    if [ -d "$SCRIPT_DIR/src/include" ]; then
        include_paths="-I$SCRIPT_DIR/src/include"
    fi
    
    # 添加系统头文件路径
    if [ -d "/usr/include" ]; then
        include_paths="$include_paths -I/usr/include"
    fi
    
    # 添加架构特定的头文件
    local arch="$(uname -m)"
    if [ -d "/usr/include/$arch-linux-gnu" ]; then
        include_paths="$include_paths -I/usr/include/$arch-linux-gnu"
    fi
    
    # 添加TCC运行时库路径
    if [ -d "$SCRIPT_DIR/lib" ]; then
        lib_paths="-L$SCRIPT_DIR/lib"
    fi
    
    # 添加系统库文件路径
    if [ -d "/usr/lib/$arch-linux-gnu" ]; then
        lib_paths="$lib_paths -L/usr/lib/$arch-linux-gnu"
    fi
    
    if [ -d "/lib/$arch-linux-gnu" ]; then
        lib_paths="$lib_paths -L/lib/$arch-linux-gnu"
    fi
    
    # 设置TCC库路径
    local tcc_lib_path=""
    if [ -d "$SCRIPT_DIR/lib" ]; then
        tcc_lib_path="-B$SCRIPT_DIR/lib"
    fi
    
    echo "$include_paths $lib_paths $tcc_lib_path"
}

# 主函数
main() {
    # 显示版本信息
    if [ "$1" = "-version" ] || [ "$1" = "--version" ]; then
        echo "TCC 辅助脚本 v1.0"
        echo "支持的架构: x86_32, x86_64, arm_32, arm_64"
        echo "支持的平台: Linux (.lnx), Windows (.exe), macOS (.osx)"
        echo ""
        echo "可用的二进制文件:"
        if [ -d "$TCC_BIN_DIR" ]; then
            for binary in "$TCC_BIN_DIR"/tcc_*; do
                if [ -f "$binary" ]; then
                    local name="$(basename "$binary")"
                    local size="$(du -h "$binary" | cut -f1)"
                    echo "  $name ($size)"
                fi
            done
        else
            echo "  无 (请先运行交叉编译脚本)"
        fi
        return 0
    fi
    
    # 显示帮助信息
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        echo "TCC 辅助脚本用法:"
        echo "  $0 [TCC选项] source.c"
        echo "  $0 -version           显示版本和可用二进制"
        echo "  $0 -help              显示此帮助"
        echo ""
        echo "示例:"
        echo "  $0 hello.c            编译 hello.c"
        echo "  $0 -o hello hello.c   编译并输出到 hello"
        echo "  $0 -run hello.c       编译并运行"
        return 0
    fi
    
    # 查找最佳的 TCC 二进制文件
    local tcc_binary=""
    tcc_binary="$(find_best_tcc)"
    if [ $? -ne 0 ] || [ -z "$tcc_binary" ]; then
        echo "错误: 找不到合适的 TCC 二进制文件" >&2
        echo "当前系统: $(detect_arch) $(detect_os)" >&2
        echo "请先运行 ./cross_compile_working.sh 来生成二进制文件" >&2
        echo "提示: 不会回退到系统TCC，只使用项目内的TCC二进制文件" >&2
        return 1
    fi
    
    # 获取系统路径
    local system_paths="$(get_system_paths)"
    
    # 执行 TCC (仅使用项目内的TCC二进制，不回退系统TCC)
    echo "使用项目TCC: $(basename "$tcc_binary")"
    exec "$tcc_binary" $system_paths "$@"
}

# 运行主函数
main "$@"