# 第六章：FFmpeg软硬解码

# 创建库
add_library(chapter6_decoder
    src/xdecode.cpp
    src/xdecode_factory.cpp
)

target_include_directories(chapter6_decoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第二章_图像格式基于QT_SDL渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第三章_FFmpeg_AVFrame渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第四章_FFmpeg像素格式转换和多路YUV_RGB渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第五章_FFmpeg视频编码/include
)

target_link_libraries(chapter6_decoder PUBLIC
    video_codec_common
    chapter2_renderer
    chapter3_avframe
    chapter4_converter
    chapter5_encoder
)

# 创建基础解码器测试
add_executable(test_xdecode
    examples/test_xdecode.cpp
)

target_link_libraries(test_xdecode
    chapter6_decoder
    video_codec_common
)

# 创建H264解码器测试
add_executable(test_h264_decoder
    examples/test_h264_decoder.cpp
)

target_link_libraries(test_h264_decoder
    chapter6_decoder
    video_codec_common
)

# 创建H265解码器测试
add_executable(test_h265_decoder
    examples/test_h265_decoder.cpp
)

target_link_libraries(test_h265_decoder
    chapter6_decoder
    video_codec_common
)

# 创建硬件解码测试
add_executable(test_hardware_decode
    examples/test_hardware_decode.cpp
)

target_link_libraries(test_hardware_decode
    chapter6_decoder
    video_codec_common
)

# 创建编解码循环测试
add_executable(test_encode_decode_loop
    examples/test_encode_decode_loop.cpp
)

target_link_libraries(test_encode_decode_loop
    chapter6_decoder
    video_codec_common
)

# 安装
install(TARGETS chapter6_decoder 
    test_xdecode test_h264_decoder test_h265_decoder
    test_hardware_decode test_encode_decode_loop
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
