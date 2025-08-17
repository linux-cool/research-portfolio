#!/bin/bash

# 视频编解码项目构建脚本

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

# 检查依赖
check_dependencies() {
    log_info "Checking dependencies..."
    
    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found. Please install CMake 3.16 or later."
        exit 1
    fi
    
    cmake_version=$(cmake --version | head -n1 | cut -d' ' -f3)
    log_info "CMake version: $cmake_version"
    
    # 检查pkg-config
    if ! command -v pkg-config &> /dev/null; then
        log_error "pkg-config not found. Please install pkg-config."
        exit 1
    fi
    
    # 检查FFmpeg
    if ! pkg-config --exists libavcodec libavformat libavutil libswscale; then
        log_error "FFmpeg development libraries not found."
        log_error "Please install FFmpeg development packages:"
        log_error "  Ubuntu/Debian: sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev"
        log_error "  CentOS/RHEL: sudo yum install ffmpeg-devel"
        log_error "  macOS: brew install ffmpeg"
        exit 1
    fi
    
    ffmpeg_version=$(pkg-config --modversion libavcodec)
    log_info "FFmpeg version: $ffmpeg_version"
    
    # 检查Qt5 (可选)
    if pkg-config --exists Qt5Core Qt5Widgets; then
        qt_version=$(pkg-config --modversion Qt5Core)
        log_info "Qt5 found: $qt_version"
    else
        log_warning "Qt5 not found. Qt features will be disabled."
    fi
    
    # 检查SDL2 (可选)
    if pkg-config --exists sdl2; then
        sdl_version=$(pkg-config --modversion sdl2)
        log_info "SDL2 found: $sdl_version"
    else
        log_warning "SDL2 not found. SDL features will be disabled."
    fi
    
    log_success "Dependencies check completed"
}

# 清理构建目录
clean_build() {
    log_info "Cleaning build directory..."
    if [ -d "build" ]; then
        rm -rf build
    fi
    mkdir -p build
    log_success "Build directory cleaned"
}

# 配置构建
configure_build() {
    log_info "Configuring build..."
    
    cd build
    
    # CMake配置选项
    CMAKE_ARGS=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )
    
    # 如果是调试模式
    if [ "$1" = "debug" ]; then
        CMAKE_ARGS[0]="-DCMAKE_BUILD_TYPE=Debug"
        log_info "Debug mode enabled"
    fi
    
    cmake "${CMAKE_ARGS[@]}" ..
    
    cd ..
    log_success "Build configured"
}

# 编译项目
build_project() {
    log_info "Building project..."
    
    cd build
    
    # 获取CPU核心数
    if command -v nproc &> /dev/null; then
        JOBS=$(nproc)
    else
        JOBS=4
    fi
    
    log_info "Using $JOBS parallel jobs"
    
    make -j$JOBS
    
    cd ..
    log_success "Build completed"
}

# 运行测试
run_tests() {
    log_info "Running tests..."
    
    cd build
    
    # 运行CTest
    if command -v ctest &> /dev/null; then
        ctest --output-on-failure
    else
        log_warning "CTest not available, skipping automated tests"
    fi
    
    cd ..
    log_success "Tests completed"
}

# 安装项目
install_project() {
    log_info "Installing project..."
    
    cd build
    sudo make install
    cd ..
    
    log_success "Installation completed"
}

# 显示帮助信息
show_help() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  build [debug]     Build the project (default: release)"
    echo "  clean             Clean build directory"
    echo "  test              Run tests"
    echo "  install           Install the project"
    echo "  all [debug]       Clean, build, and test"
    echo "  help              Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build          # Build in release mode"
    echo "  $0 build debug    # Build in debug mode"
    echo "  $0 all            # Clean, build, and test"
    echo "  $0 clean          # Clean build directory"
}

# 主函数
main() {
    local command=${1:-build}
    local mode=${2:-release}
    
    case $command in
        "build")
            check_dependencies
            clean_build
            configure_build $mode
            build_project
            ;;
        "clean")
            clean_build
            ;;
        "test")
            run_tests
            ;;
        "install")
            install_project
            ;;
        "all")
            check_dependencies
            clean_build
            configure_build $mode
            build_project
            run_tests
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            log_error "Unknown command: $command"
            show_help
            exit 1
            ;;
    esac
}

# 脚本入口
main "$@"
