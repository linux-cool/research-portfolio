# 第八章：RTSP录制
cmake_minimum_required(VERSION 3.16)

# 设置源文件
set(CHAPTER8_SOURCES
    src/xrtsp.cpp
)

set(CHAPTER8_HEADERS
    include/xrtsp.h
)

# 创建静态库
add_library(chapter8_rtsp STATIC ${CHAPTER8_SOURCES} ${CHAPTER8_HEADERS})

# 设置包含目录
target_include_directories(chapter8_rtsp PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/第七章_FFmpeg封装解封装/include
)

# 链接依赖库
target_link_libraries(chapter8_rtsp
    chapter7_muxdemux
    ${FFMPEG_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# 设置编译选项
target_compile_features(chapter8_rtsp PRIVATE cxx_std_17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(chapter8_rtsp PRIVATE DEBUG)
endif()

# 测试程序
if(BUILD_TESTS)
    # RTSP客户端测试
    add_executable(test_rtsp_client examples/test_rtsp_client.cpp)
    target_link_libraries(test_rtsp_client chapter8_rtsp)
    target_include_directories(test_rtsp_client PRIVATE include)
    
    # RTSP录制测试
    add_executable(test_rtsp_recorder examples/test_rtsp_recorder.cpp)
    target_link_libraries(test_rtsp_recorder chapter8_rtsp)
    target_include_directories(test_rtsp_recorder PRIVATE include)
    
    # RTSP多路录制测试
    add_executable(test_rtsp_multi_recorder examples/test_rtsp_multi_recorder.cpp)
    target_link_libraries(test_rtsp_multi_recorder chapter8_rtsp)
    target_include_directories(test_rtsp_multi_recorder PRIVATE include)
    
    # RTSP工具测试
    add_executable(test_rtsp_utils examples/test_rtsp_utils.cpp)
    target_link_libraries(test_rtsp_utils chapter8_rtsp)
    target_include_directories(test_rtsp_utils PRIVATE include)
    
    # RTSP性能测试
    add_executable(test_rtsp_performance examples/test_rtsp_performance.cpp)
    target_link_libraries(test_rtsp_performance chapter8_rtsp)
    target_include_directories(test_rtsp_performance PRIVATE include)
endif()

# 安装规则
install(TARGETS chapter8_rtsp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${CHAPTER8_HEADERS}
    DESTINATION include/chapter8
)
