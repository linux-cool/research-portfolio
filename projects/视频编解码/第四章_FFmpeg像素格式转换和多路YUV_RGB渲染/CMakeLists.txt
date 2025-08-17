# 第四章：FFmpeg像素格式转换和多路YUV RGB渲染

# 创建库
add_library(chapter4_converter
    src/sws_converter.cpp
    src/multi_player.cpp
)

target_include_directories(chapter4_converter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第二章_图像格式基于QT_SDL渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第三章_FFmpeg_AVFrame渲染/include
)

target_link_libraries(chapter4_converter PUBLIC
    video_codec_common
    chapter2_renderer
    chapter3_avframe
)

# 创建sws_scale转换测试
add_executable(test_sws_converter
    examples/test_sws_converter.cpp
)

target_link_libraries(test_sws_converter
    chapter4_converter
    video_codec_common
)

# 创建格式转换测试
add_executable(test_format_converter
    examples/test_format_converter.cpp
)

target_link_libraries(test_format_converter
    chapter4_converter
    video_codec_common
)

# 创建多路播放器测试
add_executable(test_multi_player
    examples/test_multi_player.cpp
)

target_link_libraries(test_multi_player
    chapter4_converter
    video_codec_common
)

# 创建批量转换测试
add_executable(test_batch_converter
    examples/test_batch_converter.cpp
)

target_link_libraries(test_batch_converter
    chapter4_converter
    video_codec_common
)

# 安装
install(TARGETS chapter4_converter 
    test_sws_converter test_format_converter 
    test_multi_player test_batch_converter
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
