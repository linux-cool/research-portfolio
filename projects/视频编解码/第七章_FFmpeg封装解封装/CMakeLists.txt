# 第七章：FFmpeg封装解封装

# 创建库
add_library(chapter7_muxdemux
    src/xdemux.cpp
    src/xdemux_factory.cpp
)

target_include_directories(chapter7_muxdemux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第二章_图像格式基于QT_SDL渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第三章_FFmpeg_AVFrame渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第四章_FFmpeg像素格式转换和多路YUV_RGB渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第五章_FFmpeg视频编码/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第六章_FFmpeg软硬解码/include
)

target_link_libraries(chapter7_muxdemux PUBLIC
    video_codec_common
    chapter2_renderer
    chapter3_avframe
    chapter4_converter
    chapter5_encoder
    chapter6_decoder
)

# 创建基础解封装测试
add_executable(test_xdemux
    examples/test_xdemux.cpp
)

target_link_libraries(test_xdemux
    chapter7_muxdemux
    video_codec_common
)

# 创建封装测试
add_executable(test_xmux
    examples/test_xmux.cpp
)

target_link_libraries(test_xmux
    chapter7_muxdemux
    video_codec_common
)

# 创建重封装测试
add_executable(test_remux
    examples/test_remux.cpp
)

target_link_libraries(test_remux
    chapter7_muxdemux
    video_codec_common
)

# 创建媒体信息测试
add_executable(test_media_info
    examples/test_media_info.cpp
)

target_link_libraries(test_media_info
    chapter7_muxdemux
    video_codec_common
)

# 创建视频剪辑测试
add_executable(test_video_clip
    examples/test_video_clip.cpp
)

target_link_libraries(test_video_clip
    chapter7_muxdemux
    video_codec_common
)

# 安装
install(TARGETS chapter7_muxdemux 
    test_xdemux test_xmux test_remux
    test_media_info test_video_clip
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
