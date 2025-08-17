# 第五章：FFmpeg视频编码

# 创建库
add_library(chapter5_encoder
    src/xencode.cpp
    src/xencode_factory.cpp
)

target_include_directories(chapter5_encoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第二章_图像格式基于QT_SDL渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第三章_FFmpeg_AVFrame渲染/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../第四章_FFmpeg像素格式转换和多路YUV_RGB渲染/include
)

target_link_libraries(chapter5_encoder PUBLIC
    video_codec_common
    chapter2_renderer
    chapter3_avframe
    chapter4_converter
)

# 创建基础编码器测试
add_executable(test_xencode
    examples/test_xencode.cpp
)

target_link_libraries(test_xencode
    chapter5_encoder
    video_codec_common
)

# 创建H264编码器测试
add_executable(test_h264_encoder
    examples/test_h264_encoder.cpp
)

target_link_libraries(test_h264_encoder
    chapter5_encoder
    video_codec_common
)

# 创建H265编码器测试
add_executable(test_h265_encoder
    examples/test_h265_encoder.cpp
)

target_link_libraries(test_h265_encoder
    chapter5_encoder
    video_codec_common
)

# 创建编码性能测试
add_executable(test_encode_performance
    examples/test_encode_performance.cpp
)

target_link_libraries(test_encode_performance
    chapter5_encoder
    video_codec_common
)

# 创建硬件加速测试
add_executable(test_hardware_encode
    examples/test_hardware_encode.cpp
)

target_link_libraries(test_hardware_encode
    chapter5_encoder
    video_codec_common
)

# 安装
install(TARGETS chapter5_encoder 
    test_xencode test_h264_encoder test_h265_encoder
    test_encode_performance test_hardware_encode
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
