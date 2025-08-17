# 第三章：FFmpeg AVFrame渲染

# 创建库
add_library(chapter3_avframe
    src/avframe_manager.cpp
    src/threaded_renderer.cpp
)

target_include_directories(chapter3_avframe PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第二章_图像格式基于QT_SDL渲染/include
)

target_link_libraries(chapter3_avframe PUBLIC
    video_codec_common
    chapter2_renderer
)

# 创建AVFrame管理测试
add_executable(test_avframe_manager
    examples/test_avframe_manager.cpp
)

target_link_libraries(test_avframe_manager
    chapter3_avframe
    video_codec_common
)

# 创建YUV转换测试
add_executable(test_yuv_converter
    examples/test_yuv_converter.cpp
)

target_link_libraries(test_yuv_converter
    chapter3_avframe
    video_codec_common
)

# 创建帧率控制测试
add_executable(test_fps_controller
    examples/test_fps_controller.cpp
)

target_link_libraries(test_fps_controller
    chapter3_avframe
    video_codec_common
)

# 创建多线程渲染测试
add_executable(test_threaded_renderer
    examples/test_threaded_renderer.cpp
)

target_link_libraries(test_threaded_renderer
    chapter3_avframe
    video_codec_common
)

# 安装
install(TARGETS chapter3_avframe 
    test_avframe_manager test_yuv_converter 
    test_fps_controller test_threaded_renderer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
