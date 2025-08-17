# 第二章：图像格式基于QT SDL渲染

# 创建库源文件列表
set(CHAPTER2_SOURCES
    src/xvideo_view_factory.cpp
)

# 如果启用Qt，添加Qt源文件
if(ENABLE_QT)
    list(APPEND CHAPTER2_SOURCES src/qt_video_view.cpp)
endif()

# 创建库
add_library(chapter2_renderer ${CHAPTER2_SOURCES})

target_include_directories(chapter2_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chapter2_renderer PUBLIC
    video_codec_common
)

# 如果启用Qt，添加Qt相关处理
if(ENABLE_QT)
    # 设置Qt MOC
    set(CMAKE_AUTOMOC ON)

    target_link_libraries(chapter2_renderer PUBLIC
        Qt5::Core
        Qt5::Widgets
    )
endif()

# 如果启用SDL，添加SDL相关处理
if(ENABLE_SDL)
    target_include_directories(chapter2_renderer PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(chapter2_renderer PUBLIC ${SDL2_LIBRARIES})
endif()

# 创建测试程序
add_executable(test_qt_sdl_renderer
    examples/test_renderer.cpp
)

target_link_libraries(test_qt_sdl_renderer
    chapter2_renderer
    video_codec_common
)

# 创建RGB渲染测试
add_executable(test_rgb_render
    examples/test_rgb_render.cpp
)

target_link_libraries(test_rgb_render
    chapter2_renderer
    video_codec_common
)

# 创建YUV渲染测试
add_executable(test_yuv_render
    examples/test_yuv_render.cpp
)

target_link_libraries(test_yuv_render
    chapter2_renderer
    video_codec_common
)

# 安装
install(TARGETS chapter2_renderer test_qt_sdl_renderer test_rgb_render test_yuv_render
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
