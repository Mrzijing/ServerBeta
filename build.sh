#!/bin/bash

# SmartHomeBetaTest 构建脚本
# Build script for SmartHomeBetaTest

set -e  # 遇到错误立即退出

echo "============================================"
echo "  SmartHomeBetaTest Build Script"
echo "============================================"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查系统依赖
check_dependencies() {
    log_info "检查系统依赖..."
    
    local deps=("cmake" "make" "g++" "pkg-config")
    local missing_deps=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing_deps+=("$dep")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "缺少以下依赖: ${missing_deps[*]}"
        echo "请运行以下命令安装依赖:"
        echo "sudo apt-get update"
        echo "sudo apt-get install -y build-essential cmake libjsoncpp-dev libsqlite3-dev libevent-dev libmosquitto-dev pkg-config"
        exit 1
    fi
    
    log_info "系统依赖检查完成"
}

# 检查开发库
check_libraries() {
    log_info "检查开发库..."
    
    # 检查 pkg-config 能否找到需要的库
    local libs=("jsoncpp" "sqlite3" "libevent" "libmosquitto")
    local missing_libs=()
    
    for lib in "${libs[@]}"; do
        if ! pkg-config --exists "$lib" 2>/dev/null; then
            missing_libs+=("$lib")
        fi
    done
    
    if [ ${#missing_libs[@]} -ne 0 ]; then
        log_warn "以下库可能未安装或未配置 pkg-config: ${missing_libs[*]}"
        log_warn "这可能不会影响编译，但如果编译失败，请检查这些库的安装状态"
    else
        log_info "开发库检查完成"
    fi
}

# 创建构建目录
create_build_dir() {
    log_info "创建构建目录..."
    
    if [ -d "build" ]; then
        log_warn "构建目录已存在，将清理旧的构建文件"
        rm -rf build/*
    else
        mkdir -p build
    fi
    
    cd build
    log_info "切换到构建目录: $(pwd)"
}

# 配置项目
configure_project() {
    log_info "配置 CMake 项目..."
    
    # 检查是否禁用 SeetaFace2
    local cmake_args=""
    if [ "$1" = "--disable-seeta" ]; then
        cmake_args="-DSEETA_ENABLED=OFF"
        log_warn "禁用 SeetaFace2 支持"
    fi
    
    if cmake $cmake_args ..; then
        log_info "CMake 配置成功"
    else
        log_error "CMake 配置失败"
        exit 1
    fi
}

# 编译项目
build_project() {
    log_info "开始编译项目..."
    
    # 获取 CPU 核心数
    local cpu_cores=$(nproc)
    log_info "使用 $cpu_cores 个核心并行编译"
    
    if make -j$cpu_cores; then
        log_info "编译成功！"
    else
        log_error "编译失败"
        exit 1
    fi
}

# 检查编译结果
check_build_result() {
    log_info "检查编译结果..."
    
    if [ -f "Server" ]; then
        local file_size=$(ls -lh Server | awk '{print $5}')
        log_info "可执行文件 'Server' 生成成功 (大小: $file_size)"
        log_info "可执行文件位置: $(pwd)/Server"
    else
        log_error "可执行文件 'Server' 未生成"
        exit 1
    fi
}

# 显示使用说明
show_usage() {
    echo ""
    log_info "构建完成！使用说明:"
    echo "1. 进入构建目录: cd build"
    echo "2. 运行服务器: ./Server"
    echo ""
    echo "注意事项:"
    echo "- 确保配置了正确的服务器 IP 地址"
    echo "- 如果启用了人脸识别，请确保 SeetaFace2 模型文件路径正确"
    echo "- 数据库文件 server.db 将在运行目录自动创建"
    echo ""
}

# 主函数
main() {
    echo "开始构建 SmartHomeBetaTest..."
    echo ""
    
    # 解析命令行参数
    local disable_seeta=""
    for arg in "$@"; do
        case $arg in
            --disable-seeta)
                disable_seeta="--disable-seeta"
                ;;
            --help|-h)
                echo "用法: $0 [选项]"
                echo "选项:"
                echo "  --disable-seeta    禁用 SeetaFace2 支持"
                echo "  --help, -h         显示此帮助信息"
                exit 0
                ;;
        esac
    done
    
    check_dependencies
    check_libraries
    create_build_dir
    configure_project $disable_seeta
    build_project
    check_build_result
    show_usage
    
    log_info "构建脚本执行完成！"
}

# 运行主函数
main "$@"