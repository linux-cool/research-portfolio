# 公共库
add_library(video_codec_common INTERFACE)

target_include_directories(video_codec_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(video_codec_common INTERFACE
    ${FFMPEG_LIBRARIES}
    Threads::Threads
)

target_compile_definitions(video_codec_common INTERFACE
    ${FFMPEG_CFLAGS_OTHER}
)

# 如果找到Qt5，添加Qt支持
if(ENABLE_QT)
    target_link_libraries(video_codec_common INTERFACE
        Qt5::Core
        Qt5::Widgets
    )
    target_compile_definitions(video_codec_common INTERFACE ENABLE_QT)
endif()

# 如果找到SDL2，添加SDL支持
if(ENABLE_SDL)
    target_include_directories(video_codec_common INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(video_codec_common INTERFACE ${SDL2_LIBRARIES})
    target_compile_definitions(video_codec_common INTERFACE ENABLE_SDL)
endif()
