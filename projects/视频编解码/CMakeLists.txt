cmake_minimum_required(VERSION 3.16)
project(VideoCodecResearch VERSION 1.0.0 LANGUAGES CXX C)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 查找依赖包
find_package(PkgConfig REQUIRED)

# FFmpeg
pkg_check_modules(FFMPEG REQUIRED
    libavcodec>=58.0.0
    libavformat>=58.0.0
    libavutil>=56.0.0
    libswscale>=5.0.0
    libswresample>=3.0.0
)

# Qt5 (暂时禁用，专注于SDL实现)
# find_package(Qt5 COMPONENTS Core Widgets QUIET)
# if(Qt5_FOUND)
#     set(ENABLE_QT ON)
#     message(STATUS "Qt5 found, enabling Qt features")
#     message(STATUS "Qt5 version: ${Qt5_VERSION}")
# else()
    set(ENABLE_QT OFF)
    message(STATUS "Qt5 disabled for now, focusing on SDL implementation")
# endif()

# SDL2 (可选)
pkg_check_modules(SDL2 QUIET sdl2>=2.0.0)
if(SDL2_FOUND)
    set(ENABLE_SDL ON)
    message(STATUS "SDL2 found, enabling SDL features")
else()
    set(ENABLE_SDL OFF)
    message(STATUS "SDL2 not found, disabling SDL features")
endif()

# 线程库
find_package(Threads REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/include)

# 添加公共库
add_subdirectory(common)

# 添加各章节
add_subdirectory(第二章_图像格式基于QT_SDL渲染)
add_subdirectory(第三章_FFmpeg_AVFrame渲染)

# TODO: 添加其他章节
# add_subdirectory(第四章_FFmpeg像素格式转换和多路YUV_RGB渲染)
# add_subdirectory(第五章_FFmpeg视频编码)
# add_subdirectory(第六章_FFmpeg视频软硬解码)
# add_subdirectory(第七章_FFmpeg封装和解封装)
# add_subdirectory(第八章_解封装rtsp并录制视频)

# 测试
enable_testing()
# TODO: 添加测试目录
# add_subdirectory(tests)

# 安装配置
install(TARGETS 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 打包配置
set(CPACK_PACKAGE_NAME "VideoCodecResearch")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Video Codec Research Project")
set(CPACK_PACKAGE_VENDOR "Research Team")

include(CPack)

# 打印配置信息
message(STATUS "=== Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "FFmpeg found: ${FFMPEG_FOUND}")
message(STATUS "Qt5 enabled: ${ENABLE_QT}")
message(STATUS "SDL2 enabled: ${ENABLE_SDL}")
message(STATUS "==============================")
